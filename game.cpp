#include"game.h"
#include"pawn.h"
#include"bishop.h"
#include"king.h"
#include"knight.h"
#include"queen.h"
#include"rook.h"
#include"fen.h"
#include<vector>
#include<string>
using namespace std;
MoveRecord lastMove;

vector<string>BoardMessage;

Game::Game()
{
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			board[i][j] = nullptr;
		}
	}
	currentTurn = WHITE;
	initBoard();
}

void Game::initBoard()
{
	for (int i = 0; i < 8; i++) {
		board[i][1] = new Pawn(WHITE, { i,1 });
	}
	board[0][0] = new Rook(WHITE, { 0,0 });
	board[1][0] = new Knight(WHITE, { 1,0 });
	board[2][0] = new Bishop(WHITE, { 2,0 });
	board[3][0] = new Queen(WHITE, { 3,0 });
	board[4][0] = new King(WHITE, { 4,0 });
	board[5][0] = new Bishop(WHITE, { 5,0 });
	board[6][0] = new Knight(WHITE, { 6,0 });
	board[7][0] = new Rook(WHITE, { 7,0 });

	for (int i = 0; i < 8; i++) {
		board[i][6] = new Pawn(BLACK, { i,6 });
	}
	board[0][7] = new Rook(BLACK, { 0,7 });
	board[1][7] = new Knight(BLACK, { 1,7 });
	board[2][7] = new Bishop(BLACK, { 2,7 });
	board[3][7] = new Queen(BLACK, { 3,7 });
	board[4][7] = new King(BLACK, { 4,7 });
	board[5][7] = new Bishop(BLACK, { 5,7 });
	board[6][7] = new Knight(BLACK, { 6,7 });
	board[7][7] = new Rook(BLACK, { 7,7 });
}



bool Game::isCheckMate()
{
	return !isKingSafe(currentTurn, board) && !hasAnyLegalMove(currentTurn, board);
}

bool Game::isStaleMate()
{
	return isKingSafe(currentTurn,board) && !hasAnyLegalMove(currentTurn, board);
}

bool Game::isDraw(vector<string>& Situation)
{
	//验证三次重复局面
	// 如果总局面数少于3次，显然不可能发生三次重复
	if (Situation.size() < 3) {
		return false;
	}

	// 获取当前（最新）的局面
	const string& currentState = Situation.back();

	// 统计当前局面在整个历史中出现的次数
	int count = std::count(Situation.begin(), Situation.end(), currentState);

	// 如果出现次数达到或超过3次，则判定为平局
	return count >= 3;
}



void Game::Record_Situation(Piece* board[8][8], vector<string>&Situation,Game& game)
{
	string fen;
	fen = generateFEN(*this);
	fen += to_string(wK_CASTLE_RIGHT(game.getBoard()));
	fen += to_string(wK_LONGCASTLE_RIGHT(game.getBoard()));
	fen += to_string(bK_CASTLE_RIGHT(game.getBoard()));
	fen += to_string(bK_LONGCASTLE_RIGHT(game.getBoard()));
	fen += to_string(game.getcurrentTurn());
	Situation.push_back(fen);
}

bool Game::hasAnyLegalMove(Color currentTurn, Piece* board[8][8])
{
	for (int startX = 0; startX < 8; startX++) {
		for (int startY = 0; startY < 8; startY++) {
			Piece* p = board[startX][startY];
			if (p != nullptr) {
				if (p->getColor() == currentTurn) {
					for (int endX = 0; endX < 8; endX++) {
						for (int endY = 0; endY < 8; endY++) {
							Position start = { startX,startY };
							Position end = { endX,endY };
							if (p->isValidMove(start, end, board) != INVALID) {
								Piece* targetPiece = board[endX][endY];
								
								if (targetPiece!=nullptr&&targetPiece->getColor() == currentTurn) {
									continue;
								}
								board[endX][endY] = p;
								board[startX][startY] = nullptr;

								bool safe = isKingSafe(currentTurn, board);

								board[startX][startY] = p;
								board[endX][endY] = targetPiece;
								if (safe) {
									return true;
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

#include <iostream>
#include <cctype> // toupper

//void Game::printBoard() {
//	cout << "\n  0 1 2 3 4 5 6 7  <- X\n";
//
//	// 从 y = 7 往下打印到 0
//	for (int y = 7; y >= 0; y--) {
//		cout << y << " ";
//		for (int x = 0; x < 8; x++) {
//			Piece* p = board[x][y];
//			if (p == nullptr) {
//				cout << ". ";
//			}
//			else {
//				char c = '?';
//				switch (p->getType()) {
//				case PAWN:   c = 'p'; break;
//				case ROOK:   c = 'r'; break;
//				case KNIGHT: c = 'n'; break;
//				case BISHOP: c = 'b'; break;
//				case QUEEN:  c = 'q'; break;
//				case KING:   c = 'k'; break;
//				}
//				// 如果是白棋，转换为大写
//				if (p->getColor() == WHITE) {
//					c = toupper(c);
//				}
//				cout << c << " ";
//			}
//		}
//		cout << y << "\n";
//	}
//	cout << "  0 1 2 3 4 5 6 7\n  ^ Y\n\n";
//}