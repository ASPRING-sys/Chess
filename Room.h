#pragma once
#define ASIO_STANDALONE

#include "game.h"
#include "crow_all.h"
#include <string>
#include <unordered_set>

class Room {
private:
    std::string room_id;
    Game chessgame;

    crow::websocket::connection* player_host = nullptr;
    crow::websocket::connection* player_black = nullptr;
    std::unordered_set<crow::websocket::connection*> spectators; // 观众席

    std::string host_uid = "";
    std::string black_uid = "";

    bool request_restart_white = false;
    bool request_restart_black = false;

public:
    // 构造与析构
    Room(std::string id);
    ~Room();

    // --- Getters (获取状态) ---
    std::string get_room_id() { return room_id; }
    crow::websocket::connection* get_host_connection() { return player_host; }
    crow::websocket::connection* get_black_connection() { return player_black; }
    std::string get_host_uid() { return host_uid; }
    std::string get_black_uid() { return black_uid; }
    bool get_request_restart_white() { return request_restart_white; }
    bool get_request_restart_black() { return request_restart_black; }

    // 核心：让外部能拿到游戏对象来走棋 (返回引用)
    Game& get_game() { return chessgame; }

    // --- Setters / Actions (改变状态) ---
    void set_host(crow::websocket::connection* conn, const std::string& uid);
    void set_black(crow::websocket::connection* conn, const std::string& uid);
    void add_spectator(crow::websocket::connection* conn);

    // 当玩家掉线时，将其从房间中移除
    void remove_connection(crow::websocket::connection* conn);

    // 检查房间里是否连一个玩家（白或黑）都没有了
    bool is_empty();

    // 重开相关的设置
    void set_restart_white(bool val) { request_restart_white = val; }
    void set_restart_black(bool val) { request_restart_black = val; }
    void reset_game(); // 真正执行重开对局的逻辑

    // 广播方法（你之前写的很好，保留）
    void broadcast(const std::string& msg) {
        if (player_host) player_host->send_text(msg);
        if (player_black) player_black->send_text(msg);
        for (auto& s : spectators) s->send_text(msg);
    }
};