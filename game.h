#pragma once
#include"pieces.h"
#include"move.h"
#include<vector>
#include<string>


struct AIMove {
	Position startPos;
	Position endPos;
	MoveMent moveType; // 从你的枚举 (MOVE, CASTLING, ENPASSANT) 借用过来
};




class Game {
private:
	Piece* board[8][8];
	Color currentTurn = WHITE;
	bool isGameOver = false;
	vector<string>Situation;
public:
	Game();

	void initBoard();//初始化棋盘

	bool isCheckMate();

	bool isStaleMate();

	bool isDraw(vector<string>&Situation);

	vector<string>& getSituation() { return Situation; }

	void Record_Situation(Piece* board[8][8], vector<string>& Situation,Game& game);

	Color getcurrentTurn() { return currentTurn; }

	auto& getBoard() { return board; }

	void changeTurn() { currentTurn = (currentTurn == WHITE) ? BLACK : WHITE;}

	bool hasAnyLegalMove(Color color,Piece*board[8][8]);

	int evaluateBoard();

	vector<AIMove> getAllLegalMoves(Color turn);

	int evaluateMating(Position friendlyKing, Position enemyKing);

	Piece* getPiece(int x, int y) {
		if (x >= 0 && x < 8 && y >= 0 && y < 8) {
			return board[x][y];
		}
		return nullptr;
	}

	// AI 专用的落子与撤销（不触发真实游戏的结算逻辑）
	void doAIMove(const AIMove& move, Piece*& capturedPiece);
	void undoAIMove(const AIMove& move, Piece* capturedPiece);

	// 核心搜索算法
	int minimax(int depth, int alpha, int beta, bool isMaximizing, Color turn);

	// 外部调用的接口
	AIMove getBestMove(int depth, Color aiColor);
};