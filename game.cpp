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
#include<algorithm>
#include <chrono> // 引入时间库
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

const int pawnTable[8][8] = {
	{0,  0,  0,  0,  0,  0,  0,  0},
	{50, 50, 50, 50, 50, 50, 50, 50},
	{10, 10, 20, 30, 30, 20, 10, 10},
	{5,  5, 10, 25, 25, 10,  5,  5},
	{0,  0,  0, 20, 20,  0,  0,  0},
	{5, -5,-10,  0,  0,-10, -5,  5},
	{5, 10, 10,-20,-20, 10, 10,  5},
	{0,  0,  0,  0,  0,  0,  0,  0}
};

const int knightTable[8][8] = {
	{-50,-40,-30,-30,-30,-30,-40,-50},
	{-40,-20,  0,  0,  0,  0,-20,-40},
	{-30,  0, 10, 15, 15, 10,  0,-30},
	{-30,  5, 15, 20, 20, 15,  5,-30},
	{-30,  0, 15, 20, 20, 15,  0,-30},
	{-30,  5, 10, 15, 15, 10,  5,-30},
	{-40,-20,  0,  5,  5,  0,-20,-40},
	{-50,-40,-30,-30,-30,-30,-40,-50}
};

const int bishopTable[8][8] = {
	{-20,-10,-10,-10,-10,-10,-10,-20},
	{-10,  0,  0,  0,  0,  0,  0,-10},
	{-10,  0,  5, 10, 10,  5,  0,-10},
	{-10,  5,  5, 10, 10,  5,  5,-10},
	{-10,  0, 10, 10, 10, 10,  0,-10},
	{-10, 10, 10, 10, 10, 10, 10,-10},
	{-10,  5,  0,  0,  0,  0,  5,-10},
	{-20,-10,-10,-10,-10,-10,-10,-20}
};

const int rookTable[8][8] = {
	{ 0,  0,  0,  0,  0,  0,  0,  0},
	{ 5, 10, 10, 10, 10, 10, 10,  5},
	{-5,  0,  0,  0,  0,  0,  0, -5},
	{-5,  0,  0,  0,  0,  0,  0, -5},
	{-5,  0,  0,  0,  0,  0,  0, -5},
	{-5,  0,  0,  0,  0,  0,  0, -5},
	{-5,  0,  0,  0,  0,  0,  0, -5},
	{ 0,  0,  0,  5,  5,  0,  0,  0}
};

const int queenTable[8][8] = {
	{-20,-10,-10, -5, -5,-10,-10,-20},
	{-10,  0,  0,  0,  0,  0,  0,-10},
	{-10,  0,  5,  5,  5,  5,  0,-10},
	{ -5,  0,  5,  5,  5,  5,  0, -5},
	{  0,  0,  5,  5,  5,  5,  0, -5},
	{-10,  5,  5,  5,  5,  5,  0,-10},
	{-10,  0,  5,  0,  0,  0,  0,-10},
	{-20,-10,-10, -5, -5,-10,-10,-20}
};

const int kingTable[8][8] = {
	{-30,-40,-40,-50,-50,-40,-40,-30},
	{-30,-40,-40,-50,-50,-40,-40,-30},
	{-30,-40,-40,-50,-50,-40,-40,-30},
	{-30,-40,-40,-50,-50,-40,-40,-30},
	{-20,-30,-30,-40,-40,-30,-30,-20},
	{-10,-20,-20,-20,-20,-20,-20,-10},
	{ 20, 20,  0,  0,  0,  0, 20, 20},
	{ 20, 30, 10,  0,  0, 10, 30, 20}
};

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

int Game::evaluateBoard() {
	int score = 0;
	int whiteMaterial = 0; // 记录白方除王以外的总子力
	int blackMaterial = 0; // 记录黑方除王以外的总子力

	Position whiteKingPos = { -1, -1 };
	Position blackKingPos = { -1, -1 };

	for (int x = 0; x < 8; x++) {
		for (int y = 0; y < 8; y++) {
			Piece* p = board[x][y];
			if (p != nullptr) {
				int pieceValue = 0;
				int pstValue = 0;

				bool isWhite = (p->getColor() == WHITE);
				int rank = isWhite ? (7 - y) : y;
				int file = x;

				switch (p->getType()) {
				case PAWN:   pieceValue = 100; pstValue = pawnTable[rank][file]; break;
				case KNIGHT: pieceValue = 320; pstValue = knightTable[rank][file]; break;
				case BISHOP: pieceValue = 330; pstValue = bishopTable[rank][file]; break;
				case ROOK:   pieceValue = 500; pstValue = rookTable[rank][file]; break;
				case QUEEN:  pieceValue = 900; pstValue = queenTable[rank][file]; break;
				case KING:
					pieceValue = 20000;
					pstValue = kingTable[rank][file];
					if (isWhite) whiteKingPos = { x, y };
					else blackKingPos = { x, y };
					break;
				}

				if (isWhite) {
					score += (pieceValue + pstValue);
					if (p->getType() != KING) whiteMaterial += pieceValue;
				}
				else {
					score -= (pieceValue + pstValue);
					if (p->getType() != KING) blackMaterial += pieceValue;
				}
			}
		}
	}

	// ==========================================
	// 【核心】阶段性评估 (Phased Evaluation)
	// ==========================================
	// 定义残局的阈值：双方除王之外的子力价值都小于 1200（大约是两车或一后以下）
	const int ENDGAME_MATERIAL = 1200;

	if (whiteMaterial < ENDGAME_MATERIAL && blackMaterial < ENDGAME_MATERIAL) {
		// 如果白方占据绝对优势（比如多一个车，领先分数 > 400）
		if (score > 400) {
			score += evaluateMating(whiteKingPos, blackKingPos);
		}
		// 如果黑方占据绝对优势
		else if (score < -400) {
			score -= evaluateMating(blackKingPos, whiteKingPos);
		}
	}

	return score;
}

vector<AIMove> Game::getAllLegalMoves(Color turn) {
	vector<AIMove> possibleMoves;

	for (int startX = 0; startX < 8; startX++) {
		for (int startY = 0; startY < 8; startY++) {
			Piece* p = board[startX][startY];
			if (p != nullptr && p->getColor() == turn) {

				for (int endX = 0; endX < 8; endX++) {
					for (int endY = 0; endY < 8; endY++) {
						if (board[endX][endY] != nullptr && board[endX][endY]->getColor() == turn) {
							continue; // 如果目标位置有自己颜色的棋子，直接放弃这个走法
						}
						Position start = { startX, startY };
						Position end = { endX, endY };

						MoveMent type = p->isValidMove(start, end, board);
						if (type != INVALID) {
							// 检查这一步走完王是否安全（你之前的核心逻辑）
							Piece* targetPiece = nullptr;
							int dir = (end.x - start.x > 0) ? 1 : -1;
							int castlingRook_X = (dir == 1) ? 7 : 0;

							// 临时做动作
							switch (type) {
							case MOVE:
								targetPiece = board[end.x][end.y];
								board[end.x][end.y] = p;
								board[start.x][start.y] = nullptr;
								break;
							case CASTLING:
								board[end.x][end.y] = p;
								board[start.x][start.y] = nullptr;
								board[start.x + dir][start.y] = board[castlingRook_X][start.y];
								board[castlingRook_X][start.y] = nullptr;
								break;
							case ENPASSANT:
								targetPiece = board[end.x][start.y];
								board[end.x][end.y] = p;
								board[start.x][start.y] = nullptr;
								board[end.x][start.y] = nullptr;
								break;
							}

							bool safe = isKingSafe(turn, board);

							// 撤销动作
							switch (type) {
							case MOVE:
								board[start.x][start.y] = p;
								board[end.x][end.y] = targetPiece;
								break;
							case CASTLING:
								board[start.x][start.y] = p;
								board[end.x][end.y] = targetPiece;
								board[castlingRook_X][start.y] = board[start.x + dir][start.y];
								board[start.x + dir][start.y] = nullptr;
								break;
							case ENPASSANT:
								board[start.x][start.y] = p;
								board[end.x][end.y] = nullptr;
								board[end.x][start.y] = targetPiece;
								break;
							}

							// 如果安全，加入到所有可用走法的列表中
							if (safe) {
								possibleMoves.push_back({ start, end, type });
							}
						}
					}
				}
			}
		}
	}
	std::sort(possibleMoves.begin(), possibleMoves.end(), [this](const AIMove& a, const AIMove& b) {
		// 快速评估棋子价值的内部 Lambda
		auto getVal = [](Piece* p) {
			if (!p) return 0;
			switch (p->getType()) {
			case PAWN: return 100; case KNIGHT: return 320;
			case BISHOP: return 330; case ROOK: return 500;
			case QUEEN: return 900; case KING: return 20000;
			}
			return 0;
			};

		int scoreA = 0, scoreB = 0;

		// 评估走法 A
		Piece* targetA = this->board[a.endPos.x][a.endPos.y];
		Piece* actorA = this->board[a.startPos.x][a.startPos.y];
		if (targetA) scoreA = 10 * getVal(targetA) - getVal(actorA); // 核心：受害者价值越高，攻击者价值越低，得分越高
		else if (a.moveType == ENPASSANT) scoreA = 10 * 100 - getVal(actorA);
		if (actorA && actorA->getType() == PAWN && (a.endPos.y == 0 || a.endPos.y == 7)) scoreA += 9000; // 兵的升变具有极高优先级

		// 评估走法 B
		Piece* targetB = this->board[b.endPos.x][b.endPos.y];
		Piece* actorB = this->board[b.startPos.x][b.startPos.y];
		if (targetB) scoreB = 10 * getVal(targetB) - getVal(actorB);
		else if (b.moveType == ENPASSANT) scoreB = 10 * 100 - getVal(actorB);
		if (actorB && actorB->getType() == PAWN && (b.endPos.y == 0 || b.endPos.y == 7)) scoreB += 9000;

		return scoreA > scoreB;
		});
	return possibleMoves;
}

void Game::doAIMove(const AIMove& move, Piece*& capturedPiece) {
	Piece* p = board[move.startPos.x][move.startPos.y];
	int dir = (move.endPos.x - move.startPos.x > 0) ? 1 : -1;
	int castlingRook_X = (dir == 1) ? 7 : 0;

	switch (move.moveType) {
	case MOVE:
		capturedPiece = board[move.endPos.x][move.endPos.y];
		board[move.endPos.x][move.endPos.y] = p;
		board[move.startPos.x][move.startPos.y] = nullptr;
		break;
	case CASTLING:
		capturedPiece = nullptr; // 王车易位不会吃子
		board[move.endPos.x][move.endPos.y] = p;
		board[move.startPos.x][move.startPos.y] = nullptr;
		board[move.startPos.x + dir][move.startPos.y] = board[castlingRook_X][move.startPos.y];
		board[castlingRook_X][move.startPos.y] = nullptr;
		break;
	case ENPASSANT:
		capturedPiece = board[move.endPos.x][move.startPos.y];
		board[move.endPos.x][move.endPos.y] = p;
		board[move.startPos.x][move.startPos.y] = nullptr;
		board[move.endPos.x][move.startPos.y] = nullptr;
		break;
	}
}

void Game::undoAIMove(const AIMove& move, Piece* capturedPiece) {
	Piece* p = board[move.endPos.x][move.endPos.y];
	int dir = (move.endPos.x - move.startPos.x > 0) ? 1 : -1;
	int castlingRook_X = (dir == 1) ? 7 : 0;

	switch (move.moveType) {
	case MOVE:
		board[move.startPos.x][move.startPos.y] = p;
		board[move.endPos.x][move.endPos.y] = capturedPiece; // 复活被吃的子
		break;
	case CASTLING:
		board[move.startPos.x][move.startPos.y] = p;
		board[move.endPos.x][move.endPos.y] = nullptr;
		board[castlingRook_X][move.startPos.y] = board[move.startPos.x + dir][move.startPos.y];
		board[move.startPos.x + dir][move.startPos.y] = nullptr;
		break;
	case ENPASSANT:
		board[move.startPos.x][move.startPos.y] = p;
		board[move.endPos.x][move.endPos.y] = nullptr;
		board[move.endPos.x][move.startPos.y] = capturedPiece;
		break;
	}
}

int Game::minimax(int depth, int alpha, int beta, bool isMaximizing, Color turn) {
	// 1. 递归的出口：到达指定深度，直接调用我们的 h(n) 打分函数
	if (depth == 0) {
		return evaluateBoard();
	}
	if (depth >= 3 && isKingSafe(turn, board)) {
		int R = 2; // 缩减深度，直接假装深度少了 2
		int eval;
		// 假装跳过当前回合，强行让对方走棋
		if (isMaximizing) {
			eval = minimax(depth - 1 - R, alpha, beta, false, (turn == WHITE) ? BLACK : WHITE);
			if (eval >= beta) return beta; // 弃权我都赢，直接剪掉这棵树！
		}
		else {
			eval = minimax(depth - 1 - R, alpha, beta, true, (turn == WHITE) ? BLACK : WHITE);
			if (eval <= alpha) return alpha;
		}
	}
	vector<AIMove> moves = getAllLegalMoves(turn);

	// 2. 边缘情况：如果已经无路可走（被将死或逼和）
	if (moves.empty()) {
		if (isKingSafe(turn, board)) return 0; // 逼和，算作 0 分平局
		// depth 越大，说明在搜索树中离现在越近（步数越少）
		// 如果我们是被将死的一方，我们希望坚持得越久越好（惩罚大）
		return isMaximizing ? -999999 - depth : 999999 + depth;
	}

	if (isMaximizing) { // 轮到白方（AI追求最高分）
		int maxEval = -9999999;
		for (const AIMove& move : moves) {
			Piece* captured = nullptr;
			doAIMove(move, captured); // 假设走这一步

			// 递归！轮到黑方走，深度减1，变成寻找极小值
			int eval = minimax(depth - 1, alpha, beta, false, (turn == WHITE) ? BLACK : WHITE);

			undoAIMove(move, captured); // 撤销这一步，恢复原状

			maxEval = max(maxEval, eval);
			alpha = max(alpha, eval);
			if (beta <= alpha) break; // Beta 剪枝：黑方之前有更好的反击，这条路不用看了
		}
		return maxEval;
	}
	else { // 轮到黑方（AI追求最低分）
		int minEval = 9999999;
		for (const AIMove& move : moves) {
			Piece* captured = nullptr;
			doAIMove(move, captured);

			int eval = minimax(depth - 1, alpha, beta, true, (turn == WHITE) ? BLACK : WHITE);

			undoAIMove(move, captured);

			minEval = min(minEval, eval);
			beta = min(beta, eval);
			if (beta <= alpha) break; // Alpha 剪枝
		}
		return minEval;
	}
}

int Game::evaluateMating(Position friendlyKing, Position enemyKing) {
	int eval = 0;

	// 1. 鼓励把敌方王逼向边缘和角落
	int enemyKingRankDist = std::max(3 - enemyKing.y, enemyKing.y - 4);
	int enemyKingFileDist = std::max(3 - enemyKing.x, enemyKing.x - 4);
	int enemyKingDistFromCenter = enemyKingRankDist + enemyKingFileDist;
	eval += enemyKingDistFromCenter * 10;

	// 2. 鼓励己方王靠近敌方王（切断退路）
	int distBetweenKings = std::abs(friendlyKing.x - enemyKing.x) + std::abs(friendlyKing.y - enemyKing.y);
	eval += (14 - distBetweenKings) * 4;

	return eval;
}


AIMove Game::getBestMove(int targetDepth, Color aiColor) {
	auto startTime = std::chrono::steady_clock::now();
	int timeLimitMs = 2000; // 设定硬性超时时间：1.5秒！你可以随意修改

	AIMove finalBestMove;
	vector<AIMove> moves = getAllLegalMoves(aiColor);

	if (moves.empty()) {
		return { {-1, -1}, {-1, -1}, MOVE };
	}

	finalBestMove = moves[0]; // 兜底
	bool isMaximizing = (aiColor == WHITE);

	// 从深度 1 开始，一层层往下搜，直到达到目标深度，或者时间耗尽！
	for (int currentDepth = 1; currentDepth <= targetDepth; currentDepth++) {
		AIMove currentBestMove = moves[0];
		int bestValue = isMaximizing ? -9999999 : 9999999;

		bool timeOut = false;

		for (const AIMove& move : moves) {
			// 每算一个分支，检查一下有没有超时
			auto now = std::chrono::steady_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
			if (duration > timeLimitMs) {
				timeOut = true;
				break; // 时间到了，赶紧逃！
			}

			Piece* captured = nullptr;
			doAIMove(move, captured);

			// 调用 minimax (注意深度传的是 currentDepth - 1)
			int boardValue = minimax(currentDepth - 1, -9999999, 9999999, !isMaximizing, (aiColor == WHITE) ? BLACK : WHITE);

			undoAIMove(move, captured);

			if (isMaximizing) {
				if (boardValue >= bestValue) {
					bestValue = boardValue;
					currentBestMove = move;
				}
			}
			else {
				if (boardValue <= bestValue) {
					bestValue = boardValue;
					currentBestMove = move;
				}
			}
		}

		// 如果在当前深度完整搜索完了（没超时），就把结果更新为最终结果
		if (!timeOut) {
			finalBestMove = currentBestMove;
			// 如果发现了绝对绝杀，就没必要继续往下搜了，直接返回
			if (bestValue > 900000 || bestValue < -900000) break;
		}
		else {
			// 如果超时了，这一层算出来的结果是残缺的，不能用。
			// 打印一行日志，告诉你最高完整搜到了第几层
			cout << "AI 搜索在深度 " << currentDepth << " 时超时，返回上一层深度 " << currentDepth - 1 << " 的最佳结果。" << endl;
			break;
		}
	}

	return finalBestMove;
}