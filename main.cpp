#define ASIO_STANDALONE
#include "crow_all.h"
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include "game.h"
#include "pieces.h"
#include "Room.h"

using namespace std;

unordered_map<std::string, Room> rooms;

unordered_map<connection*, string> conn_to_room;

// === 核心设置 ===
const string ADMIN_PASSWORD = "8888"; // [可修改] 服主专属密码！

Position stringToPos(const string& str) {
    Position pos; pos.x = str[0] - 'a'; pos.y = str[1] - '1'; return pos;
}

string generateFEN(Game& game) {
    string fen = "";
    for (int y = 7; y >= 0; y--) {
        int emptyCount = 0;
        for (int x = 0; x < 8; x++) {
            Piece* p = game.getPiece(x, y);
            if (p == nullptr) { emptyCount++; }
            else {
                if (emptyCount > 0) { fen += to_string(emptyCount); emptyCount = 0; }
                char c = 'p';
                switch (p->getType()) {
                case PAWN: c = 'p'; break; case ROOK: c = 'r'; break;
                case KNIGHT: c = 'n'; break; case BISHOP: c = 'b'; break;
                case QUEEN: c = 'q'; break; case KING: c = 'k'; break;
                }
                if (p->getColor() == WHITE) c = toupper(c);
                fen += c;
            }
        }
        if (emptyCount > 0) fen += to_string(emptyCount);
        if (y > 0) fen += "/";
    }
    return fen;
}



// 记录玩家信息的结构体
struct ClientInfo {
    string uid;
    string name;
};

int main() {
    crow::SimpleApp app;
    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> users;
    std::unordered_map<crow::websocket::connection*, ClientInfo> client_info; // 连接 -> 玩家信息

    // 身份指针与固化ID
    string host_uid = "";  // 服主(白方) UID
    string black_uid = ""; // 黑方 UID
    crow::websocket::connection* player_host = nullptr;
    crow::websocket::connection* player_black = nullptr;

    bool request_restart_white = false;
    bool request_restart_black = false;

    Game chessgame;

    // 辅助函数：专门发给服主当前的人员名单
    auto sendUserListToHost = [&]() {
        if (!player_host) return;
        crow::json::wvalue msg;
        msg["type"] = "admin_user_list";
        std::vector<crow::json::wvalue> user_list;
        for (auto& kv : client_info) {
            if (kv.first == player_host) continue; // 列表里不显示服主自己
            crow::json::wvalue u;
            u["uid"] = kv.second.uid;
            u["name"] = kv.second.name;
            u["is_black"] = (kv.first == player_black);
            user_list.push_back(std::move(u));
        }
        msg["users"] = std::move(user_list);
        player_host->send_text(msg.dump());
        };

    CROW_ROUTE(app, "/images/<string>")
        ([](const crow::request& req, crow::response& res, string filename) {
        res.set_static_file_info("images/" + filename);
        res.end();
            });

    CROW_ROUTE(app, "/")([](const crow::request& req, crow::response& res) {
        // 让 Crow 直接去读取并返回你刚才建好的 index.html 文件
        res.set_static_file_info("index.html");
        res.end();
        });

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection& conn) {
        std::lock_guard<std::mutex> _(mtx);
        users.insert(&conn);

        crow::json::wvalue state_msg;
        state_msg["type"] = "board_state";
        state_msg["fen"] = generateFEN(chessgame);
        state_msg["turn"] = (chessgame.getcurrentTurn() == WHITE) ? "white" : "black";
        conn.send_text(state_msg.dump());
            })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
        std::lock_guard<std::mutex> _(mtx);
        users.erase(&conn);
        client_info.erase(&conn); // 抹除信息

        crow::json::wvalue status_msg;
        status_msg["type"] = "opponent_status";
        status_msg["online"] = false;

        if (player_host == &conn) {
            player_host = nullptr;
            if (player_black) player_black->send_text(status_msg.dump());
        }
        if (player_black == &conn) {
            player_black = nullptr;
            if (player_host) player_host->send_text(status_msg.dump());
        }

        sendUserListToHost(); // 更新服主的控制台名单
            })
        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
        std::lock_guard<std::mutex> _(mtx);
        auto msg = crow::json::load(data);
        if (!msg) return;

        // 1. 用户进门认证与名字登记
        if (msg["type"] == "auth") {
            string uid = msg["id"].s();
            string name = msg["name"].s();
            client_info[&conn] = { uid, name }; // 登记造册

            crow::json::wvalue role_msg;
            role_msg["type"] = "role_assign";

            // 认领座位机制
            if (host_uid == uid) {
                player_host = &conn; role_msg["role"] = "host";
            }
            else if (black_uid == uid) {
                player_black = &conn; role_msg["role"] = "black";
            }
            else {
                role_msg["role"] = "spectator"; // 默认全是观众！
            }
            conn.send_text(role_msg.dump());

            crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "🔔 " + name + " 进入了大厅。";
            for (auto u : users) u->send_text(sys_msg.dump());

            if (player_host == &conn && player_black) { crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true; player_black->send_text(s.dump()); }
            if (player_black == &conn && player_host) { crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true; player_host->send_text(s.dump()); }

            sendUserListToHost();
            return;
        }

        // 2. 服主密码登录验证
        if (msg["type"] == "admin_login") {
            if (msg["pwd"].s() == ADMIN_PASSWORD) {
                host_uid = client_info[&conn].uid; // 夺舍服主身份
                player_host = &conn;

                crow::json::wvalue role_msg; role_msg["type"] = "role_assign"; role_msg["role"] = "host";
                conn.send_text(role_msg.dump());

                crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "👑 玩家 [" + client_info[&conn].name + "] 已验证为服主！";
                for (auto u : users) u->send_text(sys_msg.dump());

                sendUserListToHost();
            }
            else {
                crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "密码错误，你不是服主！";
                conn.send_text(err_msg.dump());
            }
            return;
        }

        // 3. 服主翻牌子：指定黑方
        if (msg["type"] == "admin_set_black") {
            if (&conn != player_host) return; // 防黑客伪造指令
            string target_uid = msg["target_uid"].s();

            for (auto& kv : client_info) {
                if (kv.second.uid == target_uid) {
                    black_uid = target_uid;
                    player_black = kv.first; // 正式赐座

                    crow::json::wvalue role_msg; role_msg["type"] = "role_assign"; role_msg["role"] = "black";
                    player_black->send_text(role_msg.dump());

                    crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "⚔️ 服主已指定 [" + kv.second.name + "] 作为挑战者(黑方)！";
                    for (auto u : users) u->send_text(sys_msg.dump());

                    sendUserListToHost();
                    break;
                }
            }
            return;
        }

        // 4. 服主发威：踢人
        if (msg["type"] == "admin_kick") {
            if (&conn != player_host) return;
            string target_uid = msg["target_uid"].s();
            for (auto& kv : client_info) {
                if (kv.second.uid == target_uid) {
                    crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "👢 玩家 [" + kv.second.name + "] 被服主踢出了房间。";
                    for (auto u : users) u->send_text(sys_msg.dump());

                    // 通知前端强制关闭
                    crow::json::wvalue kick_msg; kick_msg["type"] = "kicked";
                    kv.first->send_text(kick_msg.dump());

                    // 如果踢的是正在下棋的黑方，清空黑方座位
                    if (player_black == kv.first) { black_uid = ""; player_black = nullptr; }
                    break;
                }
            }
            return;
        }

        if (msg["type"] == "chat") { for (auto u : users) u->send_text(data); }

        else if (msg["type"] == "claim_win") {
            bool is_valid = false;
            if (&conn == player_host && player_black == nullptr) is_valid = true;
            if (&conn == player_black && player_host == nullptr) is_valid = true;

            if (is_valid) {
                crow::json::wvalue sys_msg; sys_msg["type"] = "system";
                sys_msg["text"] = "📢 由于对方逃跑，玩家 [" + client_info[&conn].name + "] 获得胜利！4秒后重置。";
                for (auto u : users) u->send_text(sys_msg.dump());

                std::thread([&mtx, &chessgame, &request_restart_white, &request_restart_black, &users]() {
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                    std::lock_guard<std::mutex> lock(mtx);
                    chessgame = Game(); request_restart_white = false; request_restart_black = false;
                    crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
                    for (auto u : users) u->send_text(restart_msg.dump());
                    }).detach();
            }
        }
        else if (msg["type"] == "request_restart") {
            if (&conn == player_host) request_restart_white = true;
            if (&conn == player_black) request_restart_black = true;

            if (request_restart_white && request_restart_black) {
                chessgame = Game(); request_restart_white = false; request_restart_black = false;
                crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
                for (auto u : users) u->send_text(restart_msg.dump());
            }
            else {
                crow::json::wvalue sys_msg; sys_msg["type"] = "system";
                sys_msg["text"] = "🔔 [" + client_info[&conn].name + "] 申请重新开始对局...";
                for (auto u : users) u->send_text(sys_msg.dump());
            }
        }
        else if (msg["type"] == "move") {
            if (&conn != player_host && &conn != player_black) return;

            string from_str = msg["from"].s(); string to_str = msg["to"].s();
            Position startPos = stringToPos(from_str); Position endPos = stringToPos(to_str);
            Piece* p = chessgame.getPiece(startPos.x, startPos.y);

            if (p != nullptr && p->getColor() == chessgame.getcurrentTurn()) {
                if ((p->getColor() == WHITE && &conn != player_host) ||
                    (p->getColor() == BLACK && &conn != player_black)) {
                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "这不是你的棋子！";
                    conn.send_text(err_msg.dump()); return;
                }
                if (p->Move(startPos, endPos, chessgame.getBoard())) {
                    chessgame.Record_Situation(chessgame.getBoard(), chessgame.getSituation());
                    chessgame.changeTurn();
                    crow::json::wvalue success_msg; success_msg["type"] = "move_success";
                    success_msg["fen"] = generateFEN(chessgame);
                    success_msg["turn"] = (chessgame.getcurrentTurn() == WHITE) ? "white" : "black";
                    for (auto u : users) u->send_text(success_msg.dump());

                    if (chessgame.isCheckMate() || chessgame.isStaleMate() || chessgame.isDraw(chessgame.getSituation())) {
                        crow::json::wvalue over_msg; over_msg["type"] = "system";
                        over_msg["text"] = "🏆 游戏结束！系统将在 4 秒后自动重置...";
                        for (auto u : users) u->send_text(over_msg.dump());

                        std::thread([&mtx, &chessgame, &request_restart_white, &request_restart_black, &users]() {
                            std::this_thread::sleep_for(std::chrono::seconds(4));
                            std::lock_guard<std::mutex> lock(mtx);
                            chessgame = Game(); request_restart_white = false; request_restart_black = false;
                            crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
                            for (auto u : users) u->send_text(restart_msg.dump());
                            }).detach();
                    }
                }
                else {
                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "不合法的走棋！";
                    conn.send_text(err_msg.dump());
                }
            }
        }
            });

    app.port(8080).multithreaded().run();
}
