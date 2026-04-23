#pragma once
#include<cmath>
using namespace std;
enum  PieceType {
	PAWN,
	ROOK,
	KNIGHT,
	BISHOP,
	QUEEN,
	KING
};
enum Color {
	
	WHITE,
	BLACK
};
enum MoveMent {
	MOVE,
	CASTLING,
	ENPASSANT,
	INVALID
};
struct Position {
	int x;
	int y;
};
class Piece {
public:
	Piece(Color c, PieceType t, Position p)
	{
		color = c;
		type = t;
		position = p;
		hasMoved = false;
	}
	
	virtual ~Piece() {}
	virtual MoveMent isValidMove(Position start, Position end, Piece* board[8][8]) = 0;
	bool Move(Position start, Position end, Piece* board[8][8]);
	Color getColor() const { return color; }
	PieceType getType() const { return type; }
	bool getHasMoved()const { return hasMoved; }
protected:
	Color color;
	PieceType type;
	Position position;
	bool hasMoved;//用于判断棋子是否移动过(易位,过路兵)
};

inline bool isSquareAttacked(Position pos, Color myColor, Piece* board[8][8]) {
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			Piece* attacker = board[i][j];
			if (attacker == nullptr||attacker->getColor() == myColor) {
				continue;
			}

			if (attacker->getType() == PAWN) {
				int dir = (attacker->getColor() == WHITE) ? 1 : -1;
				if (pos.y == j + dir && abs(pos.x - i) == 1) {
					return true;
				}
				continue;
			}

			Position p = { i,j };
			if (attacker->isValidMove(p, pos, board)!=INVALID) {
				return true;
			}
		}
	}
	return false;
}
