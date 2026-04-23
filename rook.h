#pragma once
//#include"move.h"
#include"pieces.h"
class Rook : public Piece {
public:
	Rook(Color c, Position p) : Piece(c, ROOK, p) {}
    MoveMent isValidMove(Position start, Position end, Piece* board[8][8]) override {
        // 车必须是直线移动
        if (start.x != end.x && start.y != end.y) {
            return INVALID;
        }

        // 计算步长方向 
        int stepX = (end.x > start.x) ? 1 : ((end.x < start.x) ? -1 : 0);
        int stepY = (end.y > start.y) ? 1 : ((end.y < start.y) ? -1 : 0);

        // 设置游标
        int currX = start.x + stepX;
        int currY = start.y + stepY;

        // 走到终点
        while (currX != end.x || currY != end.y) {
            if (board[currX][currY] != nullptr) {
                return INVALID; // 路径被挡住
            }
            currX += stepX;
            currY += stepY;
        }

        return MOVE;
    }
};
