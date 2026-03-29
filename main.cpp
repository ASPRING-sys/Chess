
#include "crow_all.h"
#include <unordered_set>
#include <mutex>
#include <iostream>
#include <string>

#include "game.h"
#include "pieces.h"

using namespace std;

Position stringToPos(const string& str) {
    Position pos;
    pos.x = str[0] - 'a';
    pos.y = str[1] - '1';
    return pos;
}

// ==========================================
// [新增] FEN 生成器：把你的二维数组快照成一行字符串
// ==========================================
string generateFEN(Game& game) {
    string fen = "";
    // FEN 规则是从第 8 行 (黑方底线 y=7) 开始往下扫，一直扫到第 1 行 (白方底线 y=0)
    for (int y = 7; y >= 0; y--) {
        int emptyCount = 0;
        for (int x = 0; x < 8; x++) {
            Piece* p = game.getPiece(x, y);
            if (p == nullptr) {
                emptyCount++; // 遇到空格子，计数器+1
            }
            else {
                if (emptyCount > 0) {
                    fen += to_string(emptyCount); // 把累积的空格数写进去
                    emptyCount = 0;
                }
                char c = 'p'; // 默认是兵
                switch (p->getType()) {
                case PAWN:   c = 'p'; break;
                case ROOK:   c = 'r'; break;
                case KNIGHT: c = 'n'; break;
                case BISHOP: c = 'b'; break;
                case QUEEN:  c = 'q'; break;
                case KING:   c = 'k'; break;
                }
                // FEN 规定：大写代表白棋，小写代表黑棋
                if (p->getColor() == WHITE) {
                    c = toupper(c);
                }
                fen += c;
            }
        }
        if (emptyCount > 0) {
            fen += to_string(emptyCount); // 补齐行末的空格
        }
        if (y > 0) fen += "/"; // 每行之间用斜杠隔开
    }
    return fen;
}

int main() {
    crow::SimpleApp app;
    std::mutex mtx;
    std::unordered_set<crow::websocket::connection*> users;

    crow::websocket::connection* player_white = nullptr;
    crow::websocket::connection* player_black = nullptr;

    Game chessgame;

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
                <title>C++ 国际象棋联机版</title>
                <link rel="stylesheet" href="https://unpkg.com/@chrisoakman/chessboardjs@1.0.0/dist/chessboard-1.0.0.min.css">
                <script src="https://code.jquery.com/jquery-3.5.1.min.js"></script>
                <script src="https://unpkg.com/@chrisoakman/chessboardjs@1.0.0/dist/chessboard-1.0.0.min.js"></script>
            </head>
            <body style="font-family: Arial; padding: 20px; display: flex; gap: 40px;">
                <div>
                    <h2>在线对局 <span id="myRole" style="font-size:16px; color:blue;">(连接中...)</span></h2>
                    <div id="board" style="width: 400px"></div>
                </div>
                <div>
                    <h2>聊天室及系统通知</h2>
                    <textarea id="chatBox" style="width: 300px; height: 300px; margin-bottom: 10px;" readonly></textarea><br>
                    <input type="text" id="msgInput" style="width: 240px; padding: 5px;" placeholder="输入消息...">
                    <button onclick="sendChat()" style="padding: 5px 15px;">发送</button>
                </div>

                <script>
                    const ws = new WebSocket('ws://' + location.host + '/ws');
                    const chatBox = document.getElementById('chatBox');
                    const msgInput = document.getElementById('msgInput');
                    const roleText = document.getElementById('myRole');
                    let board = null;
                    let myRole = 'spectator'; 

                    function logMessage(msg) {
                        chatBox.value += msg + '\n';
                        chatBox.scrollTop = chatBox.scrollHeight;
                    }

                    function onDrop (source, target) {
                        if (myRole === 'spectator') return 'snapback';
                        ws.send(JSON.stringify({ type: 'move', from: source, to: target }));
                        return 'snapback'; 
                    }

                    board = Chessboard('board', {
                        draggable: true,
                        position: 'start', // 稍后会被服务器发来的 FEN 覆盖
                        onDrop: onDrop,
                        pieceTheme: '/images/{piece}.png' 
                    });

                    ws.onmessage = function(event) {
                        const data = JSON.parse(event.data);
                        
                        if (data.type === 'chat' || data.type === 'system') {
                            logMessage(data.text);
                        } 
                        // [新增] 接收服务器发来的全量棋盘快照（用于断线重连和中途观战）
                        else if (data.type === 'board_state') {
                            board.position(data.fen, false); // false 代表瞬间摆好，不播放动画
                        }
                        else if (data.type === 'role_assign') {
                            myRole = data.role;
                            if (myRole === 'white') {
                                roleText.innerText = "(你是白方 ⚪)";
                                roleText.style.color = "green";
                            } else if (myRole === 'black') {
                                roleText.innerText = "(你是黑方 ⚫)";
                                roleText.style.color = "black";
                                board.orientation('black');
                            } else {
                                roleText.innerText = "(你是观战者 👀)";
                                roleText.style.color = "gray";
                            }
                        }
                        else if (data.type === 'move_success') {
                            board.move(data.from + '-' + data.to);
                        } 
                        else if (data.type === 'error') {
                            logMessage("❌ 错误: " + data.message);
                        }
                    };

                    function sendChat() {
                        if(msgInput.value.trim() !== "") {
                            let prefix = myRole === 'white' ? "[白方] " : (myRole === 'black' ? "[黑方] " : "[观众] ");
                            ws.send(JSON.stringify({ type: 'chat', text: prefix + msgInput.value }));
                            msgInput.value = '';
                        }
                    }

                    msgInput.addEventListener("keypress", function(event) {
                        if (event.key === "Enter") sendChat();
                    });
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

        // 1. 分配座位
        crow::json::wvalue role_msg;
        role_msg["type"] = "role_assign";
        if (player_white == nullptr) {
            player_white = &conn; role_msg["role"] = "white";
        }
        else if (player_black == nullptr) {
            player_black = &conn; role_msg["role"] = "black";
        }
        else {
            role_msg["role"] = "spectator";
        }
        conn.send_text(role_msg.dump());

        // ==========================================
        // 2. [新增] 瞬间下发当前的棋盘残局状态！
        // ==========================================
        crow::json::wvalue state_msg;
        state_msg["type"] = "board_state";
        state_msg["fen"] = generateFEN(chessgame);
        conn.send_text(state_msg.dump());

        // 3. 广播系统通知
        crow::json::wvalue sys_msg;
        sys_msg["type"] = "system";
        sys_msg["text"] = "🔔 一位玩家加入了房间。当前人数: " + to_string(users.size());
        for (auto u : users) u->send_text(sys_msg.dump());
            })
        .onclose([&](crow::websocket::connection& conn, const std::string& reason) {
        std::lock_guard<std::mutex> _(mtx);
        users.erase(&conn);

        if (player_white == &conn) {
            player_white = nullptr;
            crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "⚠️ 白方玩家断开连接！";
            for (auto u : users) u->send_text(sys_msg.dump());
        }
        else if (player_black == &conn) {
            player_black = nullptr;
            crow::json::wvalue sys_msg; sys_msg["type"] = "system"; sys_msg["text"] = "⚠️ 黑方玩家断开连接！";
            for (auto u : users) u->send_text(sys_msg.dump());
        }
            })
        .onmessage([&](crow::websocket::connection& conn, const std::string& data, bool is_binary) {
        std::lock_guard<std::mutex> _(mtx);
        auto msg = crow::json::load(data);
        if (!msg) return;

        if (msg["type"] == "chat") {
            for (auto u : users) u->send_text(data);
        }
        else if (msg["type"] == "move") {
            if (&conn != player_white && &conn != player_black) {
                crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "观战者不能走棋！";
                conn.send_text(err_msg.dump());
                return;
            }

            string from_str = msg["from"].s();
            string to_str = msg["to"].s();
            Position startPos = stringToPos(from_str);
            Position endPos = stringToPos(to_str);

            Piece* p = chessgame.getPiece(startPos.x, startPos.y);

            if (p != nullptr && p->getColor() == chessgame.getcurrentTurn()) {
                if ((p->getColor() == WHITE && &conn != player_white) ||
                    (p->getColor() == BLACK && &conn != player_black)) {
                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "这不是你的棋子！";
                    conn.send_text(err_msg.dump());
                    return;
                }

                bool moveSuccess = p->Move(startPos, endPos, chessgame.getBoard());

                if (moveSuccess) {
                    chessgame.Record_Situation(chessgame.getBoard(), chessgame.getSituation());
                    chessgame.changeTurn();

                    crow::json::wvalue success_msg;
                    success_msg["type"] = "move_success";
                    success_msg["from"] = from_str;
                    success_msg["to"] = to_str;
                    string success_str = success_msg.dump();
                    for (auto u : users) u->send_text(success_str);

                    if (chessgame.isCheckMate() || chessgame.isStaleMate() || chessgame.isDraw(chessgame.getSituation())) {
                        crow::json::wvalue over_msg; over_msg["type"] = "system"; over_msg["text"] = "🏆 游戏结束！";
                        for (auto u : users) u->send_text(over_msg.dump());
                    }

                }
                else {
                    crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "不合法的走棋，请重试！";
                    conn.send_text(err_msg.dump());
                }
            }
            else {
                crow::json::wvalue err_msg; err_msg["type"] = "error"; err_msg["message"] = "不是你的回合，或未选中正确棋子！";
                conn.send_text(err_msg.dump());
            }
        }
            });

    app.port(8080).multithreaded().run();
}
EOF