
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

using namespace std;

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

    CROW_ROUTE(app, "/")([]() {
        const char* html = R"HTML(
            <!DOCTYPE html>
            <html>
            <head>
                <meta charset="utf-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
                <title>C++ 象棋 VIP 大厅</title>
                <link rel="stylesheet" href="https://unpkg.com/@chrisoakman/chessboardjs@1.0.0/dist/chessboard-1.0.0.min.css">
                <script src="https://code.jquery.com/jquery-3.5.1.min.js"></script>
                <script src="https://unpkg.com/@chrisoakman/chessboardjs@1.0.0/dist/chessboard-1.0.0.min.js"></script>
                
                <style>
                    body { font-family: -apple-system, sans-serif; margin: 0; padding: 10px; display: flex; flex-direction: column; align-items: center; background-color: #f4f4f9; }
                    .main-container { width: 100%; max-width: 500px; display: flex; flex-direction: column; gap: 15px; }
                    .header-area { text-align: center; }
                    .header-area h2 { margin: 5px 0; font-size: 20px; color: #333; }
                    #turnIndicator { font-size: 18px; font-weight: bold; margin: 10px 0; padding: 8px; border-radius: 5px; text-align: center; background-color: #e2e3e5; color: #383d41; }
                    
                    .btn-group { display: flex; gap: 5px; justify-content: center; margin-top: 5px; flex-wrap: wrap; }
                    .btn { color: white; border: none; border-radius: 5px; padding: 8px 12px; font-size: 13px; font-weight: bold; cursor: pointer; }
                    .btn-restart { background-color: #ff9800; }
                    .btn-claim { background-color: #28a745; display: none; }
                    .btn-admin-login { background-color: #6f42c1; }
                    
                    /* [新增] 服主控制台样式 */
                    #adminPanel { display: none; background-color: #fff3cd; border: 2px solid #ffeeba; border-radius: 8px; padding: 15px; margin-top: 10px; }
                    #adminPanel h3 { margin: 0 0 10px 0; color: #856404; font-size: 16px; }
                    .user-item { display: flex; justify-content: space-between; align-items: center; padding: 5px 0; border-bottom: 1px solid #ffeeba; font-size: 14px;}
                    .user-item:last-child { border-bottom: none; }
                    .admin-btn { padding: 4px 8px; font-size: 12px; border: none; border-radius: 4px; cursor: pointer; margin-left: 5px; }
                    .set-btn { background-color: #17a2b8; color: white; }
                    .kick-btn { background-color: #dc3545; color: white; }
                    
                    #board { width: 100%; touch-action: none; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }
                    #chatBox { width: 100%; height: 150px; padding: 10px; box-sizing: border-box; border-radius: 8px; border: 1px solid #ddd; resize: none; font-size: 14px; background-color: #fff; }
                    .input-row { display: flex; gap: 10px; width: 100%; }
                    #msgInput { flex-grow: 1; padding: 12px; border-radius: 8px; border: 1px solid #ddd; font-size: 16px; }
                    .send-btn { padding: 0 20px; background-color: #007bff; color: white; border: none; border-radius: 8px; font-size: 16px; font-weight: bold; cursor: pointer; }
                </style>
            </head>
            <body>
                <div class="main-container">
                    <div class="header-area">
                        <h2>象棋 VIP 大厅 <span id="myRole" style="font-size:16px; color:#007bff;">(连接中...)</span></h2>
                        <div id="turnIndicator">🔄 同步中...</div>
                        <div class="btn-group">
                            <button class="btn btn-restart" onclick="requestRestart()">🤝 申请重开</button>
                            <button id="claimWinBtn" class="btn btn-claim" onclick="claimVictory()">🏆 对方掉线，声明胜利</button>
                            <button id="adminLoginBtn" class="btn btn-admin-login" onclick="loginAsAdmin()">👑 认证服主</button>
                        </div>
                    </div>
                    
                    <div id="adminPanel">
                        <h3>👑 上帝控制台 (点击指定你的对手)</h3>
                        <div id="userListContainer">暂无其他玩家</div>
                    </div>
                    
                    <div id="board"></div>
                    <textarea id="chatBox" readonly></textarea>
                    <div class="input-row">
                        <input type="text" id="msgInput" placeholder="输入消息...">
                        <button class="send-btn" onclick="sendChat()">发送</button>
                    </div>
                </div>

                <script>
                    const ws = new WebSocket('ws://' + location.host + '/ws');
                    const chatBox = document.getElementById('chatBox');
                    const roleText = document.getElementById('myRole');
                    const turnIndicator = document.getElementById('turnIndicator');
                    const claimWinBtn = document.getElementById('claimWinBtn');
                    const adminPanel = document.getElementById('adminPanel');
                    const adminLoginBtn = document.getElementById('adminLoginBtn');
                    const userListContainer = document.getElementById('userListContainer');
                    let board = null;
                    let myRole = 'spectator'; 

                    // 1. 获取 ID 和 昵称
                    let myPlayerId = localStorage.getItem('chess_player_id');
                    if (!myPlayerId) {
                        myPlayerId = 'usr_' + Math.random().toString(36).substr(2, 9);
                        localStorage.setItem('chess_player_id', myPlayerId);
                    }
                    let myName = localStorage.getItem('chess_player_name');
                    if (!myName) {
                        myName = prompt("欢迎来到大厅！大侠尊姓大名？") || "神秘棋手";
                        localStorage.setItem('chess_player_name', myName);
                    }

                    ws.onopen = function() {
                        ws.send(JSON.stringify({ type: 'auth', id: myPlayerId, name: myName }));
                    };

                    function logMessage(msg) {
                        chatBox.value += msg + '\n';
                        chatBox.scrollTop = chatBox.scrollHeight;
                    }

                    function updateTurnDisplay(turnColor) {
                        if (turnColor === 'white') {
                            turnIndicator.innerText = "➡️ 当前回合: 白方 ⚪";
                            turnIndicator.style.backgroundColor = "#d4edda"; turnIndicator.style.color = "#155724";
                        } else {
                            turnIndicator.innerText = "➡️ 当前回合: 黑方 ⚫";
                            turnIndicator.style.backgroundColor = "#d6d8db"; turnIndicator.style.color = "#1b1e21";
                        }
                    }

                    function onDrop (source, target) {
                        if (myRole === 'spectator') return 'snapback';
                        ws.send(JSON.stringify({ type: 'move', from: source, to: target }));
                        return 'snapback'; 
                    }

                    function requestRestart() { ws.send(JSON.stringify({ type: 'request_restart' })); }
                    function claimVictory() { ws.send(JSON.stringify({ type: 'claim_win' })); }
                    
                    // 服主认证
                    function loginAsAdmin() {
                        let pwd = prompt("请输入服主密码：");
                        if(pwd) ws.send(JSON.stringify({ type: 'admin_login', pwd: pwd }));
                    }

                    // 服主操作
                    function setOpponent(uid) { ws.send(JSON.stringify({ type: 'admin_set_black', target_uid: uid })); }
                    function kickUser(uid) { 
                        if(confirm("确定要踢出这个人吗？")) ws.send(JSON.stringify({ type: 'admin_kick', target_uid: uid })); 
                    }

                    board = Chessboard('board', { draggable: true, position: 'start', onDrop: onDrop, pieceTheme: '/images/{piece}.png' });
                    $(window).resize(board.resize);

                    ws.onmessage = function(event) {
                        const data = JSON.parse(event.data);
                        
                        if (data.type === 'chat' || data.type === 'system') {
                            logMessage(data.text);
                        } 
                        else if (data.type === 'board_state') {
                            board.position(data.fen, false); 
                            if(data.turn) updateTurnDisplay(data.turn);
                        }
                        else if (data.type === 'restart') {
                            board.start(false); logMessage("🔄 对局已重新开始！"); updateTurnDisplay('white');
                            claimWinBtn.style.display = 'none';
                        }
                        else if (data.type === 'role_assign') {
                            myRole = data.role;
                            if (myRole === 'host') {
                                roleText.innerText = "(你是服主/白方 👑)";
                                roleText.style.color = "#856404";
                                adminLoginBtn.style.display = "none";
                                adminPanel.style.display = "block"; // 显示控制台
                                myRole = 'white'; // 逻辑上依然是白方
                            } else if (myRole === 'black') {
                                roleText.innerText = "(你是黑方 ⚫)";
                                roleText.style.color = "#343a40";
                                board.orientation('black');
                                adminLoginBtn.style.display = "block";
                                adminPanel.style.display = "none";
                            } else {
                                roleText.innerText = "(你是观战者 👀)";
                                roleText.style.color = "#6c757d";
                                adminLoginBtn.style.display = "block";
                                adminPanel.style.display = "none";
                            }
                        }
                        else if (data.type === 'move_success') {
                            board.position(data.fen, true);
                            if(data.turn) updateTurnDisplay(data.turn);
                        } 
                        else if (data.type === 'opponent_status') {
                            if (myRole !== 'spectator') {
                                if (data.online === false) { claimWinBtn.style.display = 'block'; } 
                                else { claimWinBtn.style.display = 'none'; }
                            }
                        }
                        // 接收服主人员名单
                        else if (data.type === 'admin_user_list') {
                            userListContainer.innerHTML = '';
                            if(!data.users || data.users.length === 0) {
                                userListContainer.innerHTML = '暂无其他玩家'; return;
                            }
                            data.users.forEach(u => {
                                let div = document.createElement('div');
                                div.className = 'user-item';
                                let statusText = u.is_black ? ' <strong style="color:red;">[当前黑方]</strong>' : ' [观战]';
                                div.innerHTML = `<span>👤 ${u.name} ${statusText}</span>
                                    <div>
                                        <button class="admin-btn set-btn" onclick="setOpponent('${u.uid}')">设为对手</button>
                                        <button class="admin-btn kick-btn" onclick="kickUser('${u.uid}')">踢出</button>
                                    </div>`;
                                userListContainer.appendChild(div);
                            });
                        }
                        // 被踢出的残酷现实
                        else if (data.type === 'kicked') {
                            alert("你已被服主踢出房间！");
                            ws.close();
                            window.location.href = "about:blank"; // 直接白屏
                        }
                        else if (data.type === 'error') {
                            alert("❌ " + data.message);
                        }
                    };

                    function sendChat() {
                        let input = document.getElementById('msgInput');
                        if(input.value.trim() !== "") {
                            let prefix = myRole === 'white' ? "[服主] " : (myRole === 'black' ? "[黑方] " : "[观众] ");
                            ws.send(JSON.stringify({ type: 'chat', text: prefix + myName + ": " + input.value }));
                            input.value = '';
                        }
                    }
                </script>
            </body>
            </html>
        )HTML";
        return html;
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
EOF