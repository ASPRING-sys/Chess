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
#include <random>

#include "game.h"
#include "pieces.h"
#include "Room.h"

using namespace std;

struct ClientInfo { string uid; string name; };

std::mutex mtx;
unordered_map<string, Room> rooms;
unordered_map<crow::websocket::connection*, string> conn_to_room;
unordered_map<crow::websocket::connection*, ClientInfo> client_info;
unordered_map<string, string> created_rooms;

string generateRoomID() {
    const string CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    random_device rd; mt19937 gen(rd()); uniform_int_distribution<> dis(0, CHARS.size() - 1);
    string id; for (int i = 0; i < 6; ++i) id += CHARS[dis(gen)]; return id;
}

Position stringToPos(const string& str) { Position pos; pos.x = str[0] - 'a'; pos.y = str[1] - '1'; return pos; }

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

int main() {
    crow::SimpleApp app;

    auto sendUserListToHost = [&](Room& room) {
        auto host_conn = room.get_host_connection();
        if (!host_conn) return;
        crow::json::wvalue msg; msg["type"] = "admin_user_list";
        std::vector<crow::json::wvalue> user_list;
        for (auto& kv : conn_to_room) {
            if (kv.second == room.get_room_id()) {
                auto conn = kv.first; if (conn == host_conn) continue;
                crow::json::wvalue u; u["uid"] = client_info[conn].uid; u["name"] = client_info[conn].name;
                u["is_black"] = (conn == room.get_black_connection());
                user_list.push_back(std::move(u));
            }
        }
        msg["users"] = std::move(user_list); host_conn->send_text(msg.dump());
        };

    CROW_ROUTE(app, "/images/<string>")([](const crow::request& req, crow::response& res, string filename) {
        res.set_static_file_info("images/" + filename); res.end();
        });

    CROW_ROUTE(app, "/")([](const crow::request& req, crow::response& res) {
        res.set_static_file_info("index.html"); res.end();
        });

    CROW_WEBSOCKET_ROUTE(app, "/ws")
        .onopen([&](crow::websocket::connection& conn) {})
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
        std::lock_guard<std::mutex> _(mtx);
        if (conn_to_room.count(&conn) == 0) return;

        string room_id = conn_to_room[&conn];
        auto& room = rooms.at(room_id);

        bool was_host = (room.get_host_connection() == &conn);
        bool was_black = (room.get_black_connection() == &conn);

        room.remove_connection(&conn);
        conn_to_room.erase(&conn);
        client_info.erase(&conn);

        // 掉线通知 (触发前端的声明胜利按钮)
        crow::json::wvalue status_msg; status_msg["type"] = "opponent_status"; status_msg["online"] = false;
        if (was_host && room.get_black_connection()) room.get_black_connection()->send_text(status_msg.dump());
        if (was_black && room.get_host_connection()) room.get_host_connection()->send_text(status_msg.dump());

        sendUserListToHost(room);

        if (room.is_empty()) {
            // [新增] 15秒容错回收机制！不立刻删房间，等待刷新重连
            std::thread([room_id]() {
                std::this_thread::sleep_for(std::chrono::seconds(15));
                std::lock_guard<std::mutex> lock(mtx);
                // 15秒后如果房间还在，且依然是空的，才无情销毁
                if (rooms.count(room_id) && rooms.at(room_id).is_empty()) {
                    string host_uid = rooms.at(room_id).get_host_uid();
                    created_rooms.erase(host_uid);
                    rooms.erase(room_id);
                    cout << "🗑️ 房间 " << room_id << " 因长时间无人超时回收。" << endl;
                }
                }).detach();
        }
        else {
            crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "🔔 一位玩家断开了连接。";
            room.broadcast(sys_msg.dump());
        }
            })
        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
        std::lock_guard<std::mutex> _(mtx);
        auto msg = crow::json::load(data);
        if (!msg) return;

        string type = msg["type"].s();

        if (type == "auth") {
            string uid = msg["id"].s(); string name = msg["name"].s();
            string req_room = msg.has("room_id") ? msg["room_id"].s() : string("");
            client_info[&conn] = { uid, name };

            if (req_room == "") {
                if (created_rooms.count(uid)) {
                    crow::json::wvalue err; err["type"] = "error"; err["message"] = "❌ 你已经创建了房间 [" + created_rooms[uid] + "]，请先解散原房间！";
                    conn.send_text(err.dump()); return;
                }
                string new_id = generateRoomID(); rooms.emplace(new_id, Room(new_id));
                auto& room = rooms.at(new_id);
                room.set_host(&conn, uid);
                string pref_color = msg.has("color") ? msg["color"].s() : string("white"); room.set_host_color(pref_color);
                conn_to_room[&conn] = new_id; created_rooms[uid] = new_id;

                crow::json::wvalue res; res["type"] = "role_assign"; res["role"] = "host"; res["room_id"] = new_id; res["color"] = pref_color;
                conn.send_text(res.dump());
                crow::json::wvalue state; state["type"] = "board_state"; state["fen"] = generateFEN(room.get_game()); state["turn"] = "white";
                conn.send_text(state.dump());
            }
            else {
                if (rooms.count(req_room) == 0) {
                    crow::json::wvalue err; err["type"] = "error"; err["message"] = "❌ 房间号不存在或已过期解散！";
                    conn.send_text(err.dump()); return;
                }
                auto& room = rooms.at(req_room);
                conn_to_room[&conn] = req_room;

                // [新增] 断线重连核心逻辑：官复原职
                crow::json::wvalue res; res["type"] = "role_assign"; res["room_id"] = req_room;

                if (uid == room.get_host_uid()) {
                    room.set_host(&conn, uid);
                    res["role"] = "host"; res["color"] = room.get_host_color();
                    if (room.get_black_connection()) {
                        crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true;
                        room.get_black_connection()->send_text(s.dump());
                    }
                }
                else if (uid == room.get_black_uid()) {
                    room.set_black(&conn, uid);
                    res["role"] = "black"; res["color"] = (room.get_host_color() == "white") ? "black" : "white";
                    if (room.get_host_connection()) {
                        crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true;
                        room.get_host_connection()->send_text(s.dump());
                    }
                }
                else {
                    room.add_spectator(&conn);
                    res["role"] = "spectator"; res["color"] = "white";
                }

                conn.send_text(res.dump());
                crow::json::wvalue state; state["type"] = "board_state"; state["fen"] = generateFEN(room.get_game());
                state["turn"] = (room.get_game().getcurrentTurn() == WHITE) ? "white" : "black";
                conn.send_text(state.dump());

                crow::json::wvalue sys; sys["type"] = "system"; sys["text"] = "🔔 " + name + " 连接到了大厅。";
                room.broadcast(sys.dump()); sendUserListToHost(room);
            }
            return;
        }

        if (conn_to_room.count(&conn) == 0) return;
        string room_id = conn_to_room[&conn]; auto& room = rooms.at(room_id);

        if (type == "chat") { room.broadcast(data); }
        else if (type == "admin_kick") {
            if (&conn != room.get_host_connection()) return;
            string target_uid = msg["target_uid"].s();
            for (auto& kv : conn_to_room) {
                if (kv.second == room_id && client_info[kv.first].uid == target_uid) {
                    crow::json::wvalue kick_msg; kick_msg["type"] = "kicked"; kv.first->send_text(kick_msg.dump());
                    room.remove_connection(kv.first); conn_to_room.erase(kv.first);
                    crow::json::wvalue sys; sys["type"] = "system"; sys["text"] = "👢 " + client_info[kv.first].name + " 被请出了房间。";
                    room.broadcast(sys.dump()); sendUserListToHost(room); break;
                }
            }
        }
        else if (type == "admin_set_black") {
            if (&conn != room.get_host_connection()) return;
            string target_uid = msg["target_uid"].s();
            for (auto& kv : conn_to_room) {
                if (kv.second == room_id && client_info[kv.first].uid == target_uid) {
                    room.set_black(kv.first, target_uid);
                    string enemy_color = (room.get_host_color() == "white") ? "black" : "white";
                    crow::json::wvalue role_msg; role_msg["type"] = "role_assign"; role_msg["role"] = "black"; role_msg["color"] = enemy_color;
                    kv.first->send_text(role_msg.dump());
                    crow::json::wvalue sys; sys["type"] = "system"; sys["text"] = "⚔️ 房主接受了 [" + client_info[kv.first].name + "] 的挑战！";
                    room.broadcast(sys.dump());
                    crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true;
                    room.get_host_connection()->send_text(s.dump()); kv.first->send_text(s.dump());
                    sendUserListToHost(room); break;
                }
            }
        }
        // [新增] 声明胜利逻辑
        else if (type == "claim_win") {
            bool is_valid = false;
            if (&conn == room.get_host_connection() && room.get_black_connection() == nullptr) is_valid = true;
            if (&conn == room.get_black_connection() && room.get_host_connection() == nullptr) is_valid = true;

            if (is_valid) {
                crow::json::wvalue sys_msg; sys_msg["type"] = "system";
                sys_msg["text"] = "📢 对方逃跑或掉线，玩家 [" + client_info[&conn].name + "] 不战而胜！4秒后自动重置...";
                room.broadcast(sys_msg.dump());

                std::thread([room_id]() {
                    std::this_thread::sleep_for(std::chrono::seconds(4));
                    std::lock_guard<std::mutex> lock(mtx);
                    if (rooms.count(room_id)) {
                        auto& r = rooms.at(room_id); r.reset_game();
                        crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
                        r.broadcast(restart_msg.dump());
                    }
                    }).detach();
            }
        }
        // 申请重开逻辑
        else if (type == "request_restart") {
            if (&conn == room.get_host_connection()) room.set_restart_white(true);
            if (&conn == room.get_black_connection()) room.set_restart_black(true);

            if (room.get_request_restart_white() && room.get_request_restart_black()) {
                room.reset_game();
                crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
                room.broadcast(restart_msg.dump());
            }
            else {
                crow::json::wvalue sys_msg; sys_msg["type"] = "system";
                sys_msg["text"] = "🔔 [" + client_info[&conn].name + "] 申请重新开始对局 (需双方同意)...";
                room.broadcast(sys_msg.dump());
            }
        }
        else if (type == "move") {
            if (&conn != room.get_host_connection() && &conn != room.get_black_connection()) return;
            string from_str = msg["from"].s(); string to_str = msg["to"].s();
            Position startPos = stringToPos(from_str); Position endPos = stringToPos(to_str);
            Game& game = room.get_game(); Piece* p = game.getPiece(startPos.x, startPos.y);

            if (p != nullptr && p->getColor() == game.getcurrentTurn()) {
                Color host_playing_color = (room.get_host_color() == "black") ? BLACK : WHITE;
                Color black_playing_color = (room.get_host_color() == "black") ? WHITE : BLACK;
                bool can_move = false;
                if (&conn == room.get_host_connection() && p->getColor() == host_playing_color) can_move = true;
                if (&conn == room.get_black_connection() && p->getColor() == black_playing_color) can_move = true;

                if (!can_move) {
                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "这不是你的棋子！"; conn.send_text(err_msg.dump()); return;
                }

                if (p->Move(startPos, endPos, game.getBoard())) {
                    game.Record_Situation(game.getBoard(), game.getSituation());
                    game.changeTurn();
                    crow::json::wvalue success_msg; success_msg["type"] = "move_success";
                    success_msg["fen"] = generateFEN(game); success_msg["turn"] = (game.getcurrentTurn() == WHITE) ? "white" : "black";
                    room.broadcast(success_msg.dump());

                    if (game.isCheckMate() || game.isStaleMate() || game.isDraw(game.getSituation())) {
                        crow::json::wvalue over_msg; over_msg["type"] = "system";
                        over_msg["text"] = "🏆 游戏结束！系统将在 4 秒后自动重置..."; room.broadcast(over_msg.dump());
                        std::thread([room_id]() {
                            std::this_thread::sleep_for(std::chrono::seconds(4));
                            std::lock_guard<std::mutex> lock(mtx);
                            if (rooms.count(room_id)) {
                                auto& r = rooms.at(room_id); r.reset_game();
                                crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white"; r.broadcast(restart_msg.dump());
                            }
                            }).detach();
                    }
                }
                else { crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "不合法的走棋！"; conn.send_text(err_msg.dump()); }
            }
        }
            });
    app.port(8080).multithreaded().run();
}
//#define ASIO_STANDALONE
//#include "crow_all.h"
//#include <unordered_set>
//#include <unordered_map>
//#include <mutex>
//#include <iostream>
//#include <string>
//#include <thread>
//#include <chrono>
//#include <vector>
//#include <random>
//
//#include "game.h"
//#include "pieces.h"
//#include "Room.h"
//
//using namespace std;
//
//// === 核心结构与全局字典 ===
//
//struct ClientInfo {
//    string uid;
//    string name;
//};
//
//std::mutex mtx; // 全局锁，保护所有 Map 和 Room 的并发访问
//
//// 1. 房间字典: <房间号, Room对象>
//unordered_map<string, Room> rooms;
//
//// 2. 连接寻址表: <连接指针, 房间号>
//unordered_map<crow::websocket::connection*, string> conn_to_room;
//
//// 3. 玩家信息表: <连接指针, 玩家基本信息> (支持单UID多开网页)
//unordered_map<crow::websocket::connection*, ClientInfo> client_info;
//
//// 4. 房产局: <UID, 房间号> (严格限制一个UID只能【创建】一个房间)
//unordered_map<string, string> created_rooms;
//
//
//// 工具函数：生成 6 位随机房间号
//string generateRoomID() {
//    const string CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
//    random_device rd; mt19937 gen(rd());
//    uniform_int_distribution<> dis(0, CHARS.size() - 1);
//    string id;
//    for (int i = 0; i < 6; ++i) id += CHARS[dis(gen)];
//    return id;
//}
//
//// 工具函数：坐标转换与 FEN 生成
//Position stringToPos(const string& str) {
//    Position pos; pos.x = str[0] - 'a'; pos.y = str[1] - '1'; return pos;
//}
//
//string generateFEN(Game& game) {
//    string fen = "";
//    for (int y = 7; y >= 0; y--) {
//        int emptyCount = 0;
//        for (int x = 0; x < 8; x++) {
//            Piece* p = game.getPiece(x, y);
//            if (p == nullptr) { emptyCount++; }
//            else {
//                if (emptyCount > 0) { fen += to_string(emptyCount); emptyCount = 0; }
//                char c = 'p';
//                switch (p->getType()) {
//                case PAWN: c = 'p'; break; case ROOK: c = 'r'; break;
//                case KNIGHT: c = 'n'; break; case BISHOP: c = 'b'; break;
//                case QUEEN: c = 'q'; break; case KING: c = 'k'; break;
//                }
//                if (p->getColor() == WHITE) c = toupper(c);
//                fen += c;
//            }
//        }
//        if (emptyCount > 0) fen += to_string(emptyCount);
//        if (y > 0) fen += "/";
//    }
//    return fen;
//}
//
//
//int main() {
//    crow::SimpleApp app;
//
//    // 辅助函数：专门发给特定房间的服主人员名单
//    auto sendUserListToHost = [&](Room& room) {
//        auto host_conn = room.get_host_connection();
//        if (!host_conn) return;
//
//        crow::json::wvalue msg;
//        msg["type"] = "admin_user_list";
//        std::vector<crow::json::wvalue> user_list;
//
//        // 遍历整个字典，找出在同一个房间的玩家
//        for (auto& kv : conn_to_room) {
//            if (kv.second == room.get_room_id()) {
//                auto conn = kv.first;
//                if (conn == host_conn) continue; // 不显示服主自己
//
//                crow::json::wvalue u;
//                u["uid"] = client_info[conn].uid;
//                u["name"] = client_info[conn].name;
//                u["is_black"] = (conn == room.get_black_connection());
//                user_list.push_back(std::move(u));
//            }
//        }
//        msg["users"] = std::move(user_list);
//        host_conn->send_text(msg.dump());
//        };
//
//    CROW_ROUTE(app, "/images/<string>")([](const crow::request& req, crow::response& res, string filename) {
//        res.set_static_file_info("images/" + filename);
//        res.end();
//        });
//
//    CROW_ROUTE(app, "/")([](const crow::request& req, crow::response& res) {
//        res.set_static_file_info("index.html");
//        res.end();
//        });
//
//    CROW_WEBSOCKET_ROUTE(app, "/ws")
//        .onopen([&](crow::websocket::connection& conn) {
//        // 等待 auth 消息，不在此处初始化棋盘
//            })
//        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
//        std::lock_guard<std::mutex> _(mtx);
//
//        if (conn_to_room.count(&conn) == 0) return; // 还没进房间就退了
//
//        string room_id = conn_to_room[&conn];
//        auto& room = rooms.at(room_id);
//        string uid = client_info[&conn].uid;
//
//        bool was_host = (room.get_host_connection() == &conn);
//        bool was_black = (room.get_black_connection() == &conn);
//
//        // 1. 从房间中移除连接
//        room.remove_connection(&conn);
//        conn_to_room.erase(&conn);
//        client_info.erase(&conn);
//
//        // 2. 通知对手掉线
//        crow::json::wvalue status_msg; status_msg["type"] = "opponent_status"; status_msg["online"] = false;
//        if (was_host && room.get_black_connection()) room.get_black_connection()->send_text(status_msg.dump());
//        if (was_black && room.get_host_connection()) room.get_host_connection()->send_text(status_msg.dump());
//
//        sendUserListToHost(room);
//
//        // 3. 空房检测与回收机制
//        if (room.is_empty()) {
//            // 如果房间空了，释放房主的建房名额
//            created_rooms.erase(room.get_host_uid());
//            rooms.erase(room_id);
//            cout << "🗑️ 房间 " << room_id << " 已被系统回收。" << endl;
//        }
//        else {
//            crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "🔔 一位玩家断开了连接。";
//            room.broadcast(sys_msg.dump());
//        }
//            })
//        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
//        std::lock_guard<std::mutex> _(mtx);
//        auto msg = crow::json::load(data);
//        if (!msg) return;
//
//        string type = msg["type"].s();
//
//        // ================= 1. 认证与房间分配 =================
//        if (type == "auth") {
//            string uid = msg["id"].s();
//            string name = msg["name"].s();
//            string req_room = msg.has("room_id") ? msg["room_id"].s() : string("");
//
//            client_info[&conn] = { uid, name };
//
//            // 情景 A：想要建房
//            if (req_room == "") {
//                // 查房产局：是否已经建过房了？
//                if (created_rooms.count(uid)) {
//                    crow::json::wvalue err; err["type"] = "error"; err["message"] = "❌ 你已经创建了房间 [" + created_rooms[uid] + "]，请先解散原房间！";
//                    conn.send_text(err.dump());
//                    return;
//                }
//
//                // 创建新房
//                string new_id = generateRoomID();
//                rooms.emplace(new_id, Room(new_id));
//                auto& room = rooms.at(new_id);
//
//                room.set_host(&conn, uid);
//
//                // 读取房主选择的颜色 (默认白色)
//                string pref_color = msg.has("color") ? msg["color"].s() : string("white");
//                room.set_host_color(pref_color);
//
//                conn_to_room[&conn] = new_id;
//                created_rooms[uid] = new_id;
//
//                // 告诉前端：你是服主，你的具体颜色是什么
//                crow::json::wvalue res; res["type"] = "role_assign"; res["role"] = "host";
//                res["room_id"] = new_id; res["color"] = pref_color;
//                conn.send_text(res.dump());
//
//                // 发送初始棋盘
//                crow::json::wvalue state; state["type"] = "board_state"; state["fen"] = generateFEN(room.get_game());
//                state["turn"] = "white";
//                conn.send_text(state.dump());
//            }
//            // 情景 B：想要进房
//            else {
//                if (rooms.count(req_room) == 0) {
//                    crow::json::wvalue err; err["type"] = "error"; err["message"] = "❌ 房间号不存在！";
//                    conn.send_text(err.dump());
//                    return;
//                }
//
//                auto& room = rooms.at(req_room);
//                conn_to_room[&conn] = req_room;
//
//                // 默认全进观众席，等待房主提拔
//                room.add_spectator(&conn);
//
//                crow::json::wvalue res; res["type"] = "role_assign"; res["role"] = "spectator"; res["room_id"] = req_room;
//                conn.send_text(res.dump());
//
//                // 发送当前棋盘状态
//                crow::json::wvalue state; state["type"] = "board_state"; state["fen"] = generateFEN(room.get_game());
//                state["turn"] = (room.get_game().getcurrentTurn() == WHITE) ? "white" : "black";
//                conn.send_text(state.dump());
//
//                crow::json::wvalue sys; sys["type"] = "system"; sys["text"] = "🔔 " + name + " 进入了房间。";
//                room.broadcast(sys.dump());
//
//                sendUserListToHost(room);
//            }
//            return;
//        }
//
//        // ================= 拦截游魂 =================
//        if (conn_to_room.count(&conn) == 0) return;
//        string room_id = conn_to_room[&conn];
//        auto& room = rooms.at(room_id);
//
//        // ================= 2. 聊天 =================
//        if (type == "chat") { room.broadcast(data); }
//
//        // ================= 3. 服主踢人 =================
//        else if (type == "admin_kick") {
//            if (&conn != room.get_host_connection()) return;
//            string target_uid = msg["target_uid"].s();
//
//            // 在本房间找这个人
//            for (auto& kv : conn_to_room) {
//                if (kv.second == room_id && client_info[kv.first].uid == target_uid) {
//                    crow::json::wvalue kick_msg; kick_msg["type"] = "kicked";
//                    kv.first->send_text(kick_msg.dump());
//
//                    room.remove_connection(kv.first);
//                    conn_to_room.erase(kv.first); // 强制断开绑定
//
//                    crow::json::wvalue sys; sys["type"] = "system"; sys["text"] = "👢 " + client_info[kv.first].name + " 被请出了房间。";
//                    room.broadcast(sys.dump());
//                    sendUserListToHost(room);
//                    break;
//                }
//            }
//        }
//
//        // ================= 4. 服主指定对手 =================
//        else if (type == "admin_set_black") {
//            if (&conn != room.get_host_connection()) return;
//            string target_uid = msg["target_uid"].s();
//
//            for (auto& kv : conn_to_room) {
//                if (kv.second == room_id && client_info[kv.first].uid == target_uid) {
//                    room.set_black(kv.first, target_uid);
//
//                    // 告知对手他的颜色 (与房主相反)
//                    string enemy_color = (room.get_host_color() == "white") ? "black" : "white";
//
//                    crow::json::wvalue role_msg; role_msg["type"] = "role_assign";
//                    role_msg["role"] = "black"; role_msg["color"] = enemy_color;
//                    kv.first->send_text(role_msg.dump());
//
//                    crow::json::wvalue sys; sys["type"] = "system"; sys["text"] = "⚔️ 房主接受了 [" + client_info[kv.first].name + "] 的挑战！";
//                    room.broadcast(sys.dump());
//
//                    // 互相通知在线状态
//                    crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true;
//                    room.get_host_connection()->send_text(s.dump());
//                    kv.first->send_text(s.dump());
//
//                    sendUserListToHost(room);
//                    break;
//                }
//            }
//        }
//
//        // ================= 5. 下棋逻辑 (颜色解绑核心) =================
//        else if (type == "move") {
//            if (&conn != room.get_host_connection() && &conn != room.get_black_connection()) return;
//
//            string from_str = msg["from"].s(); string to_str = msg["to"].s();
//            Position startPos = stringToPos(from_str); Position endPos = stringToPos(to_str);
//
//            Game& game = room.get_game();
//            Piece* p = game.getPiece(startPos.x, startPos.y);
//
//            if (p != nullptr && p->getColor() == game.getcurrentTurn()) {
//
//                // --- 核心反转逻辑 ---
//                Color host_playing_color = (room.get_host_color() == "black") ? BLACK : WHITE;
//                Color black_playing_color = (room.get_host_color() == "black") ? WHITE : BLACK;
//
//                bool can_move = false;
//                if (&conn == room.get_host_connection() && p->getColor() == host_playing_color) can_move = true;
//                if (&conn == room.get_black_connection() && p->getColor() == black_playing_color) can_move = true;
//
//                if (!can_move) {
//                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "这不是你的棋子！";
//                    conn.send_text(err_msg.dump()); return;
//                }
//                // --------------------
//
//                if (p->Move(startPos, endPos, game.getBoard())) {
//                    game.Record_Situation(game.getBoard(), game.getSituation());
//                    game.changeTurn();
//
//                    crow::json::wvalue success_msg; success_msg["type"] = "move_success";
//                    success_msg["fen"] = generateFEN(game);
//                    success_msg["turn"] = (game.getcurrentTurn() == WHITE) ? "white" : "black";
//                    room.broadcast(success_msg.dump());
//
//                    if (game.isCheckMate() || game.isStaleMate() || game.isDraw(game.getSituation())) {
//                        crow::json::wvalue over_msg; over_msg["type"] = "system";
//                        over_msg["text"] = "🏆 游戏结束！系统将在 4 秒后自动重置...";
//                        room.broadcast(over_msg.dump());
//
//                        std::thread([room_id]() {
//                            std::this_thread::sleep_for(std::chrono::seconds(4));
//                            std::lock_guard<std::mutex> lock(mtx);
//                            if (rooms.count(room_id)) {
//                                auto& r = rooms.at(room_id);
//                                r.reset_game();
//                                crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
//                                r.broadcast(restart_msg.dump());
//                            }
//                            }).detach();
//                    }
//                }
//                else {
//                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "不合法的走棋！";
//                    conn.send_text(err_msg.dump());
//                }
//            }
//        }
//        // (其余 claim_win, request_restart 类似，只需确保调用 room.get_game() 和 room.broadcast() 即可)
//            });
//
//    app.port(8080).multithreaded().run();
//}
//#define ASIO_STANDALONE
//#include "crow_all.h"
//#include <unordered_set>
//#include <unordered_map>
//#include <mutex>
//#include <iostream>
//#include <string>
//#include <thread>
//#include <chrono>
//#include <vector>
//#include "game.h"
//#include "pieces.h"
//#include "Room.h"
//
//using namespace std;
//
//using namespace crow;
//
//using namespace websocket;
//
//unordered_map<std::string, Room> rooms;
//
//unordered_map<connection*, string> conn_to_room;
//
//// === 核心设置 ===
//const string ADMIN_PASSWORD = "8888"; // [可修改] 服主专属密码！
//
//Position stringToPos(const string& str) {
//    Position pos; pos.x = str[0] - 'a'; pos.y = str[1] - '1'; return pos;
//}
//
//string generateFEN(Game& game) {
//    string fen = "";
//    for (int y = 7; y >= 0; y--) {
//        int emptyCount = 0;
//        for (int x = 0; x < 8; x++) {
//            Piece* p = game.getPiece(x, y);
//            if (p == nullptr) { emptyCount++; }
//            else {
//                if (emptyCount > 0) { fen += to_string(emptyCount); emptyCount = 0; }
//                char c = 'p';
//                switch (p->getType()) {
//                case PAWN: c = 'p'; break; case ROOK: c = 'r'; break;
//                case KNIGHT: c = 'n'; break; case BISHOP: c = 'b'; break;
//                case QUEEN: c = 'q'; break; case KING: c = 'k'; break;
//                }
//                if (p->getColor() == WHITE) c = toupper(c);
//                fen += c;
//            }
//        }
//        if (emptyCount > 0) fen += to_string(emptyCount);
//        if (y > 0) fen += "/";
//    }
//    return fen;
//}
//
//
//
//// 记录玩家信息的结构体
//struct ClientInfo {
//    string uid;
//    string name;
//};
//
//int main() {
//    crow::SimpleApp app;
//    std::mutex mtx;
//    std::unordered_set<crow::websocket::connection*> users;
//    std::unordered_map<crow::websocket::connection*, ClientInfo> client_info; // 连接 -> 玩家信息
//
//    // 身份指针与固化ID
//    string host_uid = "";  // 服主(白方) UID
//    string black_uid = ""; // 黑方 UID
//    crow::websocket::connection* player_host = nullptr;
//    crow::websocket::connection* player_black = nullptr;
//
//    bool request_restart_white = false;
//    bool request_restart_black = false;
//
//    Game chessgame;
//
//    // 辅助函数：专门发给服主当前的人员名单
//    auto sendUserListToHost = [&]() {
//        if (!player_host) return;
//        crow::json::wvalue msg;
//        msg["type"] = "admin_user_list";
//        std::vector<crow::json::wvalue> user_list;
//        for (auto& kv : client_info) {
//            if (kv.first == player_host) continue; // 列表里不显示服主自己
//            crow::json::wvalue u;
//            u["uid"] = kv.second.uid;
//            u["name"] = kv.second.name;
//            u["is_black"] = (kv.first == player_black);
//            user_list.push_back(std::move(u));
//        }
//        msg["users"] = std::move(user_list);
//        player_host->send_text(msg.dump());
//        };
//
//    CROW_ROUTE(app, "/images/<string>")
//        ([](const crow::request& req, crow::response& res, string filename) {
//        res.set_static_file_info("images/" + filename);
//        res.end();
//            });
//
//    CROW_ROUTE(app, "/")([](const crow::request& req, crow::response& res) {
//        // 让 Crow 直接去读取并返回你刚才建好的 index.html 文件
//        res.set_static_file_info("index.html");
//        res.end();
//        });
//
//    CROW_WEBSOCKET_ROUTE(app, "/ws")
//        .onopen([&](crow::websocket::connection& conn) {
//        std::lock_guard<std::mutex> _(mtx);
//        users.insert(&conn);
//
//        crow::json::wvalue state_msg;
//        state_msg["type"] = "board_state";
//        state_msg["fen"] = generateFEN(chessgame);
//        state_msg["turn"] = (chessgame.getcurrentTurn() == WHITE) ? "white" : "black";
//        conn.send_text(state_msg.dump());
//            })
//        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
//        std::lock_guard<std::mutex> _(mtx);
//        users.erase(&conn);
//        client_info.erase(&conn); // 抹除信息
//
//        crow::json::wvalue status_msg;
//        status_msg["type"] = "opponent_status";
//        status_msg["online"] = false;
//
//        if (player_host == &conn) {
//            player_host = nullptr;
//            if (player_black) player_black->send_text(status_msg.dump());
//        }
//        if (player_black == &conn) {
//            player_black = nullptr;
//            if (player_host) player_host->send_text(status_msg.dump());
//        }
//
//        sendUserListToHost(); // 更新服主的控制台名单
//            })
//        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
//        std::lock_guard<std::mutex> _(mtx);
//        auto msg = crow::json::load(data);
//        if (!msg) return;
//
//        // 1. 用户进门认证与名字登记
//        if (msg["type"] == "auth") {
//            string uid = msg["id"].s();
//            string name = msg["name"].s();
//            client_info[&conn] = { uid, name }; // 登记造册
//
//            crow::json::wvalue role_msg;
//            role_msg["type"] = "role_assign";
//
//            // 认领座位机制
//            if (host_uid == uid) {
//                player_host = &conn; role_msg["role"] = "host";
//            }
//            else if (black_uid == uid) {
//                player_black = &conn; role_msg["role"] = "black";
//            }
//            else {
//                role_msg["role"] = "spectator"; // 默认全是观众！
//            }
//            conn.send_text(role_msg.dump());
//
//            crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "🔔 " + name + " 进入了大厅。";
//            for (auto u : users) u->send_text(sys_msg.dump());
//
//            if (player_host == &conn && player_black) { crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true; player_black->send_text(s.dump()); }
//            if (player_black == &conn && player_host) { crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true; player_host->send_text(s.dump()); }
//
//            sendUserListToHost();
//            return;
//        }
//
//        // 2. 服主密码登录验证
//        if (msg["type"] == "admin_login") {
//            if (msg["pwd"].s() == ADMIN_PASSWORD) {
//                host_uid = client_info[&conn].uid; // 夺舍服主身份
//                player_host = &conn;
//
//                crow::json::wvalue role_msg; role_msg["type"] = "role_assign"; role_msg["role"] = "host";
//                conn.send_text(role_msg.dump());
//
//                crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "👑 玩家 [" + client_info[&conn].name + "] 已验证为服主！";
//                for (auto u : users) u->send_text(sys_msg.dump());
//
//                sendUserListToHost();
//            }
//            else {
//                crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "密码错误，你不是服主！";
//                conn.send_text(err_msg.dump());
//            }
//            return;
//        }
//
//        // 3. 服主翻牌子：指定黑方
//        if (msg["type"] == "admin_set_black") {
//            if (&conn != player_host) return; // 防黑客伪造指令
//            string target_uid = msg["target_uid"].s();
//
//            for (auto& kv : client_info) {
//                if (kv.second.uid == target_uid) {
//                    black_uid = target_uid;
//                    player_black = kv.first; // 正式赐座
//
//                    crow::json::wvalue role_msg; role_msg["type"] = "role_assign"; role_msg["role"] = "black";
//                    player_black->send_text(role_msg.dump());
//
//                    crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "⚔️ 服主已指定 [" + kv.second.name + "] 作为挑战者(黑方)！";
//                    for (auto u : users) u->send_text(sys_msg.dump());
//
//                    sendUserListToHost();
//                    break;
//                }
//            }
//            return;
//        }
//
//        // 4. 服主发威：踢人
//        if (msg["type"] == "admin_kick") {
//            if (&conn != player_host) return;
//            string target_uid = msg["target_uid"].s();
//            for (auto& kv : client_info) {
//                if (kv.second.uid == target_uid) {
//                    crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "👢 玩家 [" + kv.second.name + "] 被服主踢出了房间。";
//                    for (auto u : users) u->send_text(sys_msg.dump());
//
//                    // 通知前端强制关闭
//                    crow::json::wvalue kick_msg; kick_msg["type"] = "kicked";
//                    kv.first->send_text(kick_msg.dump());
//
//                    // 如果踢的是正在下棋的黑方，清空黑方座位
//                    if (player_black == kv.first) { black_uid = ""; player_black = nullptr; }
//                    break;
//                }
//            }
//            return;
//        }
//
//        if (msg["type"] == "chat") { for (auto u : users) u->send_text(data); }
//
//        else if (msg["type"] == "claim_win") {
//            bool is_valid = false;
//            if (&conn == player_host && player_black == nullptr) is_valid = true;
//            if (&conn == player_black && player_host == nullptr) is_valid = true;
//
//            if (is_valid) {
//                crow::json::wvalue sys_msg; sys_msg["type"] = "system";
//                sys_msg["text"] = "📢 由于对方逃跑，玩家 [" + client_info[&conn].name + "] 获得胜利！4秒后重置。";
//                for (auto u : users) u->send_text(sys_msg.dump());
//
//                std::thread([&mtx, &chessgame, &request_restart_white, &request_restart_black, &users]() {
//                    std::this_thread::sleep_for(std::chrono::seconds(4));
//                    std::lock_guard<std::mutex> lock(mtx);
//                    chessgame = Game(); request_restart_white = false; request_restart_black = false;
//                    crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
//                    for (auto u : users) u->send_text(restart_msg.dump());
//                    }).detach();
//            }
//        }
//        else if (msg["type"] == "request_restart") {
//            if (&conn == player_host) request_restart_white = true;
//            if (&conn == player_black) request_restart_black = true;
//
//            if (request_restart_white && request_restart_black) {
//                chessgame = Game(); request_restart_white = false; request_restart_black = false;
//                crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
//                for (auto u : users) u->send_text(restart_msg.dump());
//            }
//            else {
//                crow::json::wvalue sys_msg; sys_msg["type"] = "system";
//                sys_msg["text"] = "🔔 [" + client_info[&conn].name + "] 申请重新开始对局...";
//                for (auto u : users) u->send_text(sys_msg.dump());
//            }
//        }
//        else if (msg["type"] == "move") {
//            if (&conn != player_host && &conn != player_black) return;
//
//            string from_str = msg["from"].s(); string to_str = msg["to"].s();
//            Position startPos = stringToPos(from_str); Position endPos = stringToPos(to_str);
//            Piece* p = chessgame.getPiece(startPos.x, startPos.y);
//
//            if (p != nullptr && p->getColor() == chessgame.getcurrentTurn()) {
//                if ((p->getColor() == WHITE && &conn != player_host) ||
//                    (p->getColor() == BLACK && &conn != player_black)) {
//                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "这不是你的棋子！";
//                    conn.send_text(err_msg.dump()); return;
//                }
//                if (p->Move(startPos, endPos, chessgame.getBoard())) {
//                    chessgame.Record_Situation(chessgame.getBoard(), chessgame.getSituation());
//                    chessgame.changeTurn();
//                    crow::json::wvalue success_msg; success_msg["type"] = "move_success";
//                    success_msg["fen"] = generateFEN(chessgame);
//                    success_msg["turn"] = (chessgame.getcurrentTurn() == WHITE) ? "white" : "black";
//                    for (auto u : users) u->send_text(success_msg.dump());
//
//                    if (chessgame.isCheckMate() || chessgame.isStaleMate() || chessgame.isDraw(chessgame.getSituation())) {
//                        crow::json::wvalue over_msg; over_msg["type"] = "system";
//                        over_msg["text"] = "🏆 游戏结束！系统将在 4 秒后自动重置...";
//                        for (auto u : users) u->send_text(over_msg.dump());
//
//                        std::thread([&mtx, &chessgame, &request_restart_white, &request_restart_black, &users]() {
//                            std::this_thread::sleep_for(std::chrono::seconds(4));
//                            std::lock_guard<std::mutex> lock(mtx);
//                            chessgame = Game(); request_restart_white = false; request_restart_black = false;
//                            crow::json::wvalue restart_msg; restart_msg["type"] = "restart"; restart_msg["turn"] = "white";
//                            for (auto u : users) u->send_text(restart_msg.dump());
//                            }).detach();
//                    }
//                }
//                else {
//                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "不合法的走棋！";
//                    conn.send_text(err_msg.dump());
//                }
//            }
//        }
//            });
//
//    app.port(8080).multithreaded().run();
//}
