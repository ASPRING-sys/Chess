
#include "pieces.h"
#include "queen.h"
#include "rook.h"
#include "bishop.h"
#include "knight.h"
#include <iostream>

using namespace std;

extern MoveRecord lastMove;//用于记录上一步

bool Piece::Move(Position start, Position end, Piece* board[8][8]) {
	//越界
	if (start.x > 7 || start.y > 7) {
		return false;
	}
	if (start.x < 0 || start.y < 0) {
		return false;
	}
	if (end.x > 7 || end.y > 7) {
		return false;
	}
	if (end.x < 0 || end.y < 0) {
		return false;
	}
	if (start.x == end.x && start.y == end.y) {//没有移动
		return false;
	}
	if (board[start.x][start.y] == nullptr) {//起始位置无棋子
		return false;
	}
	if (board[end.x][end.y] != nullptr) {//终点存在己方棋子
		if (board[end.x][end.y]->getColor() == this->getColor()) {
			return false;
		}
	}
	//if (this->isValidMove(start, end, board)== INVALID) {
	//	cout << "Invalid move for this piece!";
	//	return false;
	//}
	MoveMent moveType = this->isValidMove(start, end, board);
	if (moveType == INVALID) {
		cout << "Invalid move for this piece!" << endl;
		return false;
	}

	Piece* targetPiece = nullptr;

	//CASTLING专属变量
	int dir = (end.x - start.x > 0) ? 1 : -1;
	int castlingRook_X = (dir == 1) ? 7 : 0;

	switch (moveType) {
		//模拟将棋子移动到目标位置
	case MOVE:
		targetPiece = board[end.x][end.y];
		board[end.x][end.y] = this;
		board[start.x][start.y] = nullptr;
		break;
	case CASTLING:
		board[end.x][end.y] = this;
		board[start.x][start.y] = nullptr;
		board[start.x + dir][start.y] = board[castlingRook_X][start.y];
		board[castlingRook_X][start.y] = nullptr;
		break;
	case ENPASSANT:
		targetPiece = board[end.x][start.y];
		board[end.x][end.y] = this;
		board[start.x][start.y] = nullptr;
		board[end.x][start.y] = nullptr;
		break;
	}



	bool safe = isKingSafe(this->getColor(), board);

	if (safe) {
		//真正提交走法
		//并销毁被吃掉的棋子
		//记录这一步
		this->position = end;
		this->hasMoved = true;
		if (targetPiece != nullptr) {
			delete targetPiece;
		}
		lastMove.pieceMoved = this;
		lastMove.startPos = start;
		lastMove.endPos = end;

		if (moveType == CASTLING) {
			if (board[start.x + dir][start.y]) {
				board[start.x + dir][start.y]->position = { start.x + dir, start.y };
				board[start.x + dir][start.y]->hasMoved = true;
			}
			else {
				return false;
			}
		}
		
		return true;
	}
	else {
		//王被攻击
		//将之前模拟的走法恢复原状
		switch (moveType) {
		case MOVE:
			board[start.x][start.y] = this;
			board[end.x][end.y] = targetPiece;
			break;
		case CASTLING:
			board[start.x][start.y] = this;
			board[end.x][end.y] = targetPiece;
			board[castlingRook_X][start.y] = board[start.x + dir][start.y];
			board[start.x + dir][start.y] = nullptr;
			break;
		case ENPASSANT:
			board[start.x][start.y] = this;
			board[end.x][end.y] = nullptr;
			board[end.x][start.y] = targetPiece;
			break;
		}
		return false;
	}
}