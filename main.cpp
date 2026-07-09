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
#include"fen.h"
#include "game.h"
#include "pieces.h"
#include "Room.h"
#include "pawn.h"
using namespace std;

struct ClientInfo { string uid; string name; };

std::mutex mtx;
unordered_map<string, Room> rooms;
unordered_map<crow::websocket::connection*, string> conn_to_room;
unordered_map<crow::websocket::connection*, ClientInfo> client_info;
unordered_map<string, string> created_rooms;

//生成随机6位ID作为房间号
string generateRoomID() {
    const string CHARS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    random_device rd; mt19937 gen(rd()); uniform_int_distribution<> dis(0, CHARS.size() - 1);
    string id; for (int i = 0; i < 6; ++i) id += CHARS[dis(gen)]; return id;
}
//将字符串如e4,d4等更符合直觉的棋步转换为坐标
Position stringToPos(const string& str) { Position pos; pos.x = str[0] - 'a'; pos.y = str[1] - '1'; return pos; }

string posToString(Position pos) {
    string str = "";
    str += (char)('a' + pos.x);
    str += (char)('1' + pos.y);
    return str;
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
                u["is_black"] = (conn == room.get_guest_connection());
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
        bool was_black = (room.get_guest_connection() == &conn);

        room.remove_connection(&conn);
        conn_to_room.erase(&conn);
        client_info.erase(&conn);

        // 掉线通知 (触发前端的声明胜利按钮)
        crow::json::wvalue status_msg; status_msg["type"] = "opponent_status"; status_msg["online"] = false;
        if (was_host && room.get_guest_connection()) room.get_guest_connection()->send_text(status_msg.dump());
        if (was_black && room.get_host_connection()) room.get_host_connection()->send_text(status_msg.dump());

        sendUserListToHost(room);

        if (room.is_empty()) {
            // [新增] 15秒容错回收机制！不立刻删房间，等待刷新重连
            std::thread([room_id]() {
                std::this_thread::sleep_for(std::chrono::seconds(15));
                std::lock_guard<std::mutex> lock(mtx);
                // 15秒后如果房间还在，且依然是空的，才销毁
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
                    if (room.get_guest_connection()) {
                        crow::json::wvalue s; s["type"] = "opponent_status"; s["online"] = true;
                        room.get_guest_connection()->send_text(s.dump());
                    }
                }
                else if (uid == room.get_guest_uid()) {
                    room.set_guest(&conn, uid);
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
                    room.set_guest(kv.first, target_uid);
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
            if (&conn == room.get_host_connection() && room.get_guest_connection() == nullptr) is_valid = true;
            if (&conn == room.get_guest_connection() && room.get_host_connection() == nullptr) is_valid = true;

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
            if (&conn == room.get_guest_connection()) room.set_restart_guest(true);

            if (room.get_request_restart_white() && room.get_request_restart_guest()) {
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

        else if (type == "ask_hint") {
            // 1. 只有对局双方可以请求提示，观战者不行
            if (&conn != room.get_host_connection() && &conn != room.get_guest_connection()) return;

            Game& game = room.get_game();

            // 判断请求者是白方还是黑方
            Color my_color = WHITE;
            // 引入布尔标志位：只有当房主存在且没有对手加入时，沙盒模式才成立
            bool is_sandbox_mode = (&conn == room.get_host_connection() && room.get_guest_connection() == nullptr);

            if (is_sandbox_mode) {
                // 如果是单机沙盒模式，打破身份隔离，直接把当前应该走棋的阵营颜色赋给 my_color
                my_color = game.getcurrentTurn();
            }
            else {
                // 正常的联机对战逻辑，严格判断身份，防止越权操作
                if (&conn == room.get_host_connection()) {
                    my_color = (room.get_host_color() == "white") ? WHITE : BLACK;
                }
                else if (&conn == room.get_guest_connection()) {
                    my_color = (room.get_host_color() == "white") ? BLACK : WHITE;
                }

                // 2. 联机模式下，只能在自己的回合请求提示
                if (my_color != game.getcurrentTurn()) {
                    crow::json::wvalue err_msg;
                    err_msg["type"] = "error";
                    err_msg["message"] = "现在不是你的回合，请等待对方落子。";
                    conn.send_text(err_msg.dump());
                    return;
                }
            }

            // 给客户端发个消息，告诉他 AI 正在思考（防止以为卡顿）
            crow::json::wvalue sys_msg;
            sys_msg["type"] = "system";
            sys_msg["text"] = "🧠 AI 正在推演棋局，请稍候...";
            conn.send_text(sys_msg.dump());

            // 3. 调用启发式搜索算法！(设置搜索深度为3)
            // 这行代码会调用你之前写好的 minimax 函数
            AIMove bestMove = game.getBestMove(6, my_color);

            // 合法性校验：确保 AI 返回的着法真实存在于当前合法走法列表中
            vector<AIMove> legalMoves = game.getAllLegalMoves(my_color);
            bool isLegal = false;
            for (const auto& m : legalMoves) {
                if (m.startPos.x == bestMove.startPos.x && m.startPos.y == bestMove.startPos.y &&
                    m.endPos.x == bestMove.endPos.x && m.endPos.y == bestMove.endPos.y) {
                    isLegal = true;
                    break;
                }
            }

            if (!isLegal) {
                crow::json::wvalue err_msg;
                err_msg["type"] = "error";
                err_msg["message"] = "⚠️ AI 无法找到有效提示（当前局面可能已无路可走或陷入绝杀）。";
                conn.send_text(err_msg.dump());
                return;
            }
            // 4. 将算出来的坐标转换并回传给该玩家
            crow::json::wvalue hint_msg;
            hint_msg["type"] = "hint_result";
            hint_msg["from"] = posToString(bestMove.startPos);
            hint_msg["to"] = posToString(bestMove.endPos);
            conn.send_text(hint_msg.dump());
        }

        else if (type == "move") {
            if (&conn != room.get_host_connection() && &conn != room.get_guest_connection()) return;
            string from_str = msg["from"].s(); string to_str = msg["to"].s();
            Position startPos = stringToPos(from_str); Position endPos = stringToPos(to_str);
            Game& game = room.get_game(); Piece* p = game.getPiece(startPos.x, startPos.y);

            if (p != nullptr && p->getColor() == game.getcurrentTurn()) {
                Color host_playing_color = (room.get_host_color() == "black") ? BLACK : WHITE;
                Color guest_playing_color = (room.get_host_color() == "black") ? WHITE : BLACK;
                bool can_move = false;
                if (&conn == room.get_host_connection() && room.get_guest_connection() == nullptr) {
                    can_move = true;
                }
                else {
                    // 正常的联机对战权限校验
                    if (&conn == room.get_host_connection() && p->getColor() == host_playing_color) can_move = true;
                    if (&conn == room.get_guest_connection() && p->getColor() == guest_playing_color) can_move = true;
                }

                if (!can_move) {
                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "这不是你的棋子！"; conn.send_text(err_msg.dump()); return;
                }

                if (p->Move(startPos, endPos, game.getBoard())) {
                    if (p->getType() == PAWN && static_cast<Pawn*>(p)->is_promotion(endPos)) {

                        // 先广播一个临时的状态，让前端把兵移动到最后一行（不然前端的兵还停在原地）
                        crow::json::wvalue temp_msg; temp_msg["type"] = "move_success";
                        temp_msg["fen"] = generateFEN(game);
                        temp_msg["turn"] = (game.getcurrentTurn() == WHITE) ? "white" : "black";
                        room.broadcast(temp_msg.dump());

                        // 通知当前走棋的玩家进行升变选择
                        crow::json::wvalue promo_msg;
                        promo_msg["type"] = "need_promotion";
                        promo_msg["pos"] = to_str; // 把坐标发给前端，告诉它是哪个格子要升变
                        conn.send_text(promo_msg.dump());

                        // 🚨 核心：直接 return！不要记录棋局，不要切换回合，不要检查将死！
                        return;
                    }
                    game.Record_Situation(game.getBoard(), game.getSituation(),game);
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
        // [新增] 2. 处理前端传回来的升变选择
        else if (type == "promote") {
            // 安全校验：只有下棋的双方才能发这个指令
            if (&conn != room.get_host_connection() && &conn != room.get_guest_connection()) return;

            string pos_str = msg["pos"].s();
            string target = msg["target"].s(); // 期望前端传回 "q" (后), "r" (车), "b" (象), "n" (马)
            Position pos = stringToPos(pos_str);

            Game& game = room.get_game();
            Piece* p = game.getPiece(pos.x, pos.y);

            // 严谨校验：确保该位置真的有个兵，且确实是当前回合玩家的兵
            if (p != nullptr && p->getType() == PAWN && p->getColor() == game.getcurrentTurn()) {

                PieceType targetType = QUEEN; // 默认升后
                if (target == "r") targetType = ROOK;
                else if (target == "b") targetType = BISHOP;
                else if (target == "n") targetType = KNIGHT;

                // 调用你在 pawn.h 中完善的 promotion 函数 (需传入 targetType 动态 new 不同的棋子)
                static_cast<Pawn*>(p)->promotion(targetType, pos, game.getBoard());

                // 升变完成后，继续之前被打断的回合结算！
                game.Record_Situation(game.getBoard(), game.getSituation(),game);
                game.changeTurn();

                crow::json::wvalue success_msg; success_msg["type"] = "move_success";
                success_msg["fen"] = generateFEN(game);
                success_msg["turn"] = (game.getcurrentTurn() == WHITE) ? "white" : "black";
                room.broadcast(success_msg.dump()); // 广播最终变成新棋子后的 FEN 码

                // 继续之前的游戏结束判定
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
            }
            });
    app.port(8080).multithreaded().run();
}
