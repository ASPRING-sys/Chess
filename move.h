#pragma once
#include "pieces.h"
#include"bishop.h"
#include"knight.h"
#include"queen.h"
#include"rook.h"
#include<iostream>
using namespace std;


struct MoveRecord {
	Piece* pieceMoved;
	Position startPos;
	Position endPos;
};
inline bool isKingSafe(Color c, Piece* board[8][8]);

inline bool isKingSafe(Color c,Piece*board[8][8]) {
	Position kingPos = { -1,-1 };

	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			Piece* p = board[i][j];
			if (p != nullptr && p->getColor() == c && p->getType() == KING) {
				kingPos.x = i;
				kingPos.y = j;
				break;
			}
		}
	}
	if (kingPos.x == -1) {
		return false;
	}
	for (int i = 0; i < 8; i++) {
		for (int j = 0; j < 8; j++) {
			Piece* attacker = board[i][j];
			if (attacker != nullptr && attacker->getColor() != c) {
				Position attackerPos = { i,j };
				if (attacker->isValidMove(attackerPos, kingPos, board)!=INVALID) {
					return false;
				}
			}
		}
	}
	return true;
}