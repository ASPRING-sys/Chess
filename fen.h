#pragma once
#include"pieces.h"
#include"game.h"

bool wK_CASTLE_RIGHT(Piece* board[8][8])
{
    if (board[4][0]->getColor() == WHITE && board[4][0]->getType() == KING && board[4][0]->getHasMoved() == false
        && board[7][0]->getColor() == WHITE && board[7][0]->getType() == ROOK && board[7][0]->getHasMoved() == false) {
        return true;
    }
    else {
        return false;
    }
}

bool wK_LONGCASTLE_RIGHT(Piece* board[8][8])
{
    if (board[4][0]->getColor() == WHITE && board[4][0]->getType() == KING && board[4][0]->getHasMoved() == false
        && board[0][0]->getColor() == WHITE && board[0][0]->getType() == ROOK && board[0][0]->getHasMoved() == false) {
        return true;
    }
    else {
        return false;
    }
}

bool bK_CASTLE_RIGHT(Piece* board[8][8])
{
    if (board[4][7]->getColor() == BLACK && board[4][7]->getType() == KING && board[4][7]->getHasMoved() == false
        && board[7][7]->getColor() == BLACK && board[7][7]->getType() == ROOK && board[7][7]->getHasMoved() == false) {
        return true;
    }
    else {
        return false;
    }
}

bool bK_LONGCASTLE_RIGHT(Piece* board[8][8])
{
    if (board[4][7]->getColor() == BLACK && board[4][7]->getType() == KING && board[4][7]->getHasMoved() == false
        && board[0][7]->getColor() == BLACK && board[0][7]->getType() == ROOK && board[0][7]->getHasMoved() == false) {
        return true;
    }
    else {
        return false;
    }
}

//生成FEN字符串记录棋盘状态
string generateFEN(Game& game) {
    string fen = "";
    for (int y = 7; y >= 0; y--) {
        int emptyCount = 0;
        for (int x = 0; x < 8; x++) {
            Piece* p = game.getPiece(x, y);
            if (p == nullptr) { emptyCount++; }
            else {
                if (emptyCount > 0) { fen += to_string(emptyCount); emptyCount = 0; }
                char c = 'p';
                switch (p->getType()) {
                case PAWN: c = 'p'; break; case ROOK: c = 'r'; break;
                case KNIGHT: c = 'n'; break; case BISHOP: c = 'b'; break;
                case QUEEN: c = 'q'; break; case KING: c = 'k'; break;
                }
                if (p->getColor() == WHITE) c = toupper(c);
                fen += c;
            }
        }
        if (emptyCount > 0) fen += to_string(emptyCount);
        if (y > 0) fen += "/";
    }
    
    return fen;
}