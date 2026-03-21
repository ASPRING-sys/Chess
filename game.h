#pragma once
#include"pieces.h"
#include"move.h"
class Game {
private:
	Piece* board[8][8];
	Color currentTurn = WHITE;
	bool isGameOver = false;
public:
	Game();
	void initBoard();
	void startTerminalLoop();
	void printBoard();
	bool isCheckMate();
	bool isStaleMate();
	bool isDraw();
	Color getcurrentTurn() { return currentTurn; }
	auto& getBoard() { return board; }
	void changeTurn() { currentTurn = (currentTurn == WHITE) ? BLACK : WHITE;}
	bool hasAnyLegalMove(Color color,Piece*board[8][8]);
	Piece* getPiece(int x, int y) {
		if (x >= 0 && x < 8 && y >= 0 && y < 8) {
			return board[x][y];
		}
		return nullptr;
	}
};