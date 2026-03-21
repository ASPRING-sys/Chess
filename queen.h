#pragma once
#include"move.h"
#include"pieces.h"
class Queen : public Piece {
public:
	Queen(Color c, Position p) : Piece(c, QUEEN, p) {}

	MoveMent isValidMove(Position start, Position end, Piece* board[8][8]) override {
		// 1. 验证是否符合皇后的移动方向（直线 或 对角线）
		bool isStraight = (start.x == end.x || start.y == end.y);
		bool isDiagonal = (abs(start.x - end.x) == abs(start.y - end.y));

		if (!isStraight && !isDiagonal) {
			return INVALID; // 既不是直线也不是斜线，不合法
		}

		// 计算步长 
		int stepX = (end.x > start.x) ? 1 : ((end.x < start.x) ? -1 : 0);
		int stepY = (end.y > start.y) ? 1 : ((end.y < start.y) ? -1 : 0);

		// 设置游标
		int currX = start.x + stepX;
		int currY = start.y + stepY;

		// 走到终点
		while (currX != end.x || currY != end.y) {
			if (board[currX][currY] != nullptr) {
				return INVALID; // 路径被挡住了
			}
			currX += stepX;
			currY += stepY;
		}

		return MOVE;
	}
};


