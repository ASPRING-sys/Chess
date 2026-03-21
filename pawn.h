#pragma once
#include"move.h"
#include"pieces.h"

extern MoveRecord lastMove; 

class Pawn :public Piece {
public:
	Pawn(Color c, Position p) :Piece(c, PAWN, p) {}

	MoveMent isValidMove(Position start, Position end, Piece* board[8][8])override {
		//假设白兵的y值较小,黑兵的y值较大,即白方在棋盘下方,黑方在上方
		//当兵的y值为其初始值时,可以走两步,否则只能走一步
		int dir = 0;
		if (this->getColor() == WHITE) {
			dir = 1;
		}
		else {
			dir = -1;
		}
		//直走
		if (start.x == end.x) {

			//在初始位置
			if ((start.y == 1 && dir == 1) || (start.y == 6 && dir == -1)) {
				//走一步
				if (end.y - start.y == dir) {
					if (board[end.x][end.y] != nullptr) {
						return INVALID;
					}
					else {
						return MOVE;
					}
				}
				//走两步
				if (end.y - start.y == dir * 2) {
					if (board[start.x][start.y + dir] != nullptr || board[end.x][end.y] != nullptr) {
						return INVALID;
					}
					else {
						return MOVE;
					}
				}
				return INVALID;
			}
			//不在初始位置
			else {
				if (end.y - start.y == dir) {
					if (board[end.x][end.y] != nullptr) {
						return INVALID;
					}
					else {
						return MOVE;
					}
				}
				return INVALID;
			}
		}
		//斜吃
		if (abs(start.x - end.x) == 1) {
			if (end.y == start.y + dir) {
				if (board[end.x][end.y] != nullptr) {
					return MOVE;
				}
				else {
					Piece* victim = board[end.x][start.y];

					if (victim != nullptr &&
						victim->getColor() != this->getColor() &&
						victim->getType() == PAWN) {
						if (lastMove.pieceMoved == victim &&
							abs(lastMove.endPos.y - lastMove.startPos.y) == 2 &&
							lastMove.endPos.x == end.x &&
							lastMove.endPos.y == start.y) {
							return ENPASSANT;
						}
					}
				}
			}
		}
		return INVALID;
	}

};