#pragma once
//#include"move.h"
#include"pieces.h"
class Bishop :public Piece {
public:
	Bishop(Color c, Position p) :Piece(c, BISHOP, p) {}
	MoveMent isValidMove(Position start, Position end, Piece* board[8][8])override {
		//只能斜着走
		if (abs(start.x - end.x) != abs(start.y - end.y)) {
			return INVALID;
		}
		//判断路径上是否存在棋子
		int stepX = (end.x > start.x) ? 1 : -1;
		int stepY = (end.y > start.y) ? 1 : -1;

		int currX = start.x + stepX;
		int currY = start.y + stepY;

		while (currX != end.x && currY != end.y) {
			if (board[currX][currY] != nullptr) {
				return INVALID;
			}
			currX += stepX;
			currY += stepY;
		}

		return MOVE;
	}
};
