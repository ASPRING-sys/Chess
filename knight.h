#pragma once
#include"move.h"
#include"pieces.h"

class Knight :public Piece {
public:
	Knight(Color c, Position p) :Piece(c, KNIGHT, p) {}
	MoveMent isValidMove(Position start, Position end, Piece* board[8][8])override {
		//骑士的移动,在x和y方向上一个移动1单位,另一个移动2单位
		int i = abs(start.x - end.x);
		int j = abs(start.y - end.y);
		if (i == 1 && j == 2) {
			return MOVE;
		}
		else if (i == 2 && j == 1) {
			return MOVE;
		}
		else {
			return INVALID;
		}
	}
};