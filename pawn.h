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

	bool is_promotion(Position end) {
		if (this->getColor() == WHITE && end.y == 7) {
			return true;
		}
		else if (this->getColor() == BLACK && end.y == 0) {
			return true;
		}
	}

	// 建议修改为接收一个 PieceType 参数
	void promotion(PieceType targetType, Position end, Piece* board[8][8]) {
		Color myColor = this->getColor();

		// 1. 释放原位置（升变点）的旧棋子内存（即当前的兵）
		// 注意：通常在 main 逻辑中，兵已经移动到了 end 位置
		if (board[end.x][end.y] != nullptr) {
			delete board[end.x][end.y];
		}

		// 2. 根据玩家选择实例化新对象
		switch (targetType) {
		case QUEEN:  board[end.x][end.y] = new Queen(myColor, end); break;
		case ROOK:   board[end.x][end.y] = new Rook(myColor, end); break;
		case BISHOP: board[end.x][end.y] = new Bishop(myColor, end); break;
		case KNIGHT: board[end.x][end.y] = new Knight(myColor, end); break;
		default:     board[end.x][end.y] = new Queen(myColor, end); break; // 默认给后
		}
	}

};