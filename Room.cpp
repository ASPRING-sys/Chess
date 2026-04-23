#include "Room.h"

using namespace std;
using namespace crow::websocket;

// 构造函数：初始化房间号
Room::Room(string id) {
    this->room_id = id;
}

Room::~Room() {
    // 析构函数，如果房间被销毁，可以做一些清理工作
}

void Room::set_host(connection* conn, const string& uid) {
    player_host = conn;
    host_uid = uid;
    // 如果这个人以前在观众席，要把他从观众席踢出去
    spectators.erase(conn);
}

void Room::set_guest(connection* conn, const string& uid) {
    player_guest = conn;
    guest_uid = uid;
    spectators.erase(conn);
}

void Room::add_spectator(connection* conn) {
    spectators.insert(conn);
}

// 核心逻辑：有人掉线了，我们要在房间里抹除他的痕迹
void Room::remove_connection(connection* conn) {
    if (player_host == conn) {
        player_host = nullptr;
        // host_uid 不清空，让他断线重连之后还能找回身份
    }
    else if (player_guest == conn) {
        player_guest = nullptr;
    }
    else {
        spectators.erase(conn);
    }
}

// 判断房间是否成了空房（方便后续服务器自动销毁空房间）
bool Room::is_empty() {
    return (player_host == nullptr && player_guest == nullptr && spectators.empty());
}

// 重开对局逻辑
void Room::reset_game() {
    chessgame = Game(); // 重新实例化一个棋局
    request_restart_white = false;
    request_restart_guest = false;
}