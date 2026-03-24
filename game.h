#pragma once
#include"pieces.h"
#include"move.h"
#include<vector>
#include<string>



class Game {
private:
	Piece* board[8][8];
	Color currentTurn = WHITE;
	bool isGameOver = false;
	vector<string>Situation;
public:
	Game();

	void initBoard();//初始化棋盘

	void startTerminalLoop();

	void printBoard();

	bool isCheckMate();

	bool isStaleMate();

	bool isDraw(vector<string>&Situation);

	vector<string>& getSituation() { return Situation; }

	void Record_Situation(Piece* board[8][8], vector<string>& Situation);

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