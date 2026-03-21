#pragma once
#include"move.h"
#include"pieces.h"
class King :public Piece {
public:
	King(Color c, Position p) :Piece(c, KING, p) {}
	MoveMent isValidMove(Position start, Position end, Piece* board[8][8])override {
		//判断易位意图
		if (abs(start.x - end.x) == 2 && (end.y == start.y)) {
			if (this->getHasMoved() || !isKingSafe(this->getColor(), board)) {
				return INVALID;
			}
			int stepX = (end.x > start.x) ? 1 : -1;
			int expectedRookX = (stepX == 1) ? 7 : 0;

			Piece* targetRook = board[expectedRookX][start.y];

			if (targetRook == nullptr ||
				targetRook->getType() != ROOK ||
				targetRook->getColor() != this->getColor() ||
				targetRook->getHasMoved()) {
				return INVALID;
			}
			//检查路径上是否有阻挡
			int currX = start.x + stepX;
			while (currX != expectedRookX) {
				if (board[currX][start.y] != nullptr) {
					return INVALID;
				}
				currX += stepX;
			}
			//检查路径是否被攻击
			
			// 检查国王经过的格子和最终到达的格子是否被攻击
			Position p1 = { start.x + stepX, start.y };
			Position p2 = { end.x, start.y };
			if (isSquareAttacked(p1, this->color, board) || isSquareAttacked(p2, this->color, board)) {
				return INVALID;
			}
			
			return CASTLING;
		}
		//常规移动
		int i = abs(start.x - end.x);
		int j = abs(start.y - end.y);
		if (i <= 1 && j <= 1) {
			return MOVE;
		}
		else {
			return INVALID;
		}
	}
};