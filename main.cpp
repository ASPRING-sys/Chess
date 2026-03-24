#include <SFML/Graphics.hpp>
#include <iostream>
#include <map>
#include<string>
#include <vector>
#include "game.h" 
#include "pieces.h"

using namespace std;

int main()
{
	Game chessgame;

	sf::RenderWindow window(sf::VideoMode(800, 800), "My Chess Engine");
	float squareSize = 100.f;

	sf::Texture tex_wP, tex_wR, tex_wN, tex_wB, tex_wQ, tex_wK;
	sf::Texture tex_bP, tex_bR, tex_bN, tex_bB, tex_bQ, tex_bK;

	tex_wP.loadFromFile("images/chess_piece_2_white_pawn.png");
	tex_wR.loadFromFile("images/chess_piece_2_white_rook.png");
	tex_wN.loadFromFile("images/chess_piece_2_white_knight.png");
	tex_wB.loadFromFile("images/chess_piece_2_white_bishop.png");
	tex_wQ.loadFromFile("images/chess_piece_2_white_queen.png");
	tex_wK.loadFromFile("images/chess_piece_2_white_king.png");

	tex_bP.loadFromFile("images/bP.png");
	tex_bR.loadFromFile("images/bR.png");
	tex_bN.loadFromFile("images/bN.png");
	tex_bB.loadFromFile("images/bB.png");
	tex_bQ.loadFromFile("images/bQ.png");
	tex_bK.loadFromFile("images/bK.png");

	sf::Sprite pieceSprite;
	pieceSprite.setScale(2, 2);

	sf::RectangleShape square(sf::Vector2f(squareSize, squareSize));

	sf::Color lightColor(240, 217, 181);
	sf::Color darkColor(181, 136, 99);

	bool isDragging = false;
	Piece* draggedPiece = nullptr;
	Position startPos = { -1, -1 };
	sf::Vector2f offset;


	sf::Font font;
	if (!font.loadFromFile("arial.ttf")) {
		cout << "Failed to load font!" << endl;
	}

	sf::Text gameOverText;
	gameOverText.setFont(font);
	gameOverText.setCharacterSize(60);
	gameOverText.setFillColor(sf::Color::Red);
	gameOverText.setStyle(sf::Text::Bold);

	bool isGameOver = false;
	string gameOverMessage = "";
	// ==========================================
	// 【游戏主循环开始】
	// ==========================================
	while (window.isOpen()) {
		sf::Event event;

		// 1. 处理所有事件
		while (window.pollEvent(event)) {
			if (event.type == sf::Event::Closed) {
				window.close();
			}

			// 抓起棋子
			if (event.type == sf::Event::MouseButtonPressed) {

				if (isGameOver)continue;

				if (event.mouseButton.button == sf::Mouse::Left) {
					sf::Vector2i mousePos = sf::Mouse::getPosition(window);

					int x = mousePos.x / (int)squareSize;
					int y = mousePos.y / (int)squareSize;
					y = 7 - y; // 翻转Y轴

					if (x >= 0 && x < 8 && y >= 0 && y < 8) {
						Piece* p = chessgame.getPiece(x, y);
						// 确保点中的是当前回合的棋子
						if (p != nullptr && p->getColor() == chessgame.getcurrentTurn()) {
							isDragging = true;
							draggedPiece = p;
							startPos = { x, y };

							sf::Vector2f piecePixelPos(x * squareSize, (7 - y) * squareSize);
							offset = sf::Vector2f(mousePos.x, mousePos.y) - piecePixelPos;
						}
					}
				}
			}

			// 放下棋子
			if (event.type == sf::Event::MouseButtonReleased) {
				if (event.mouseButton.button == sf::Mouse::Left) {
					if (isDragging && draggedPiece != nullptr) {
						sf::Vector2i mousePos = sf::Mouse::getPosition(window);

						int x = mousePos.x / (int)squareSize;
						int y = mousePos.y / (int)squareSize;
						y = 7 - y;

						Position endPos = { x, y };

						// 调用核心引擎
						bool moveSuccess = draggedPiece->Move(startPos, endPos, chessgame.getBoard());

						if (moveSuccess) {
							chessgame.Record_Situation(chessgame.getBoard(), chessgame.getSituation());
							cout << "Move logic success!" << endl;
							// 移动成功后，切换回合
							chessgame.changeTurn(); 

							if (chessgame.isCheckMate()) {
								isGameOver = true;

								if (chessgame.getcurrentTurn() == BLACK) {
									gameOverMessage = "Checkmate!\nWhite Wins!";
								}
								else {
									gameOverMessage = "Checkmate!\nBlack Wins!";
								}
								cout << "Game Over: " << gameOverMessage << endl;
							}
							else if (chessgame.isStaleMate()) {
								isGameOver = true;
								gameOverMessage = "Stalemate!\nDraw!";
								cout << "Game Over: " << gameOverMessage << endl;
							}
							else if (chessgame.isDraw(chessgame.getSituation())) {
								isGameOver = true;
								gameOverMessage = "Draw!\n";
								cout << "Game Over: " << gameOverMessage << endl;
							}
						}
						else {
							cout << "Invalid move, snapping back." << endl;
						}

						// 清理状态
						isDragging = false;
						draggedPiece = nullptr;
						startPos = { -1, -1 };
					}
				}
			}
		} // 循环结束

		// ==========================================
		// 2. 画面绘制
		// ==========================================
		window.clear();

		// 画棋盘
		for (int row = 0; row < 8; row++) {
			for (int col = 0; col < 8; col++) {
				if ((row + col) % 2 == 0) square.setFillColor(lightColor);
				else square.setFillColor(darkColor);

				square.setPosition(col * squareSize, row * squareSize);
				window.draw(square);
			}
		}

		// 画棋子
		for (int x = 0; x < 8; x++) {
			for (int y = 0; y < 8; y++) {
				Piece* p = chessgame.getPiece(x, y);

				if (p != nullptr) {
					// 正在被拖拽的棋子先隐藏，留到最后画
					if (isDragging && p == draggedPiece) continue;

					// 设置纹理
					if (p->getColor() == WHITE) {
						switch (p->getType()) {
						case PAWN:   pieceSprite.setTexture(tex_wP); break;
						case ROOK:   pieceSprite.setTexture(tex_wR); break;
						case KNIGHT: pieceSprite.setTexture(tex_wN); break;
						case BISHOP: pieceSprite.setTexture(tex_wB); break;
						case QUEEN:  pieceSprite.setTexture(tex_wQ); break;
						case KING:   pieceSprite.setTexture(tex_wK); break;
						}
					}
					else {
						switch (p->getType()) {
						case PAWN:   pieceSprite.setTexture(tex_bP); break;
						case ROOK:   pieceSprite.setTexture(tex_bR); break;
						case KNIGHT: pieceSprite.setTexture(tex_bN); break;
						case BISHOP: pieceSprite.setTexture(tex_bB); break;
						case QUEEN:  pieceSprite.setTexture(tex_bQ); break;
						case KING:   pieceSprite.setTexture(tex_bK); break;
						}
					}

					pieceSprite.setPosition(x * squareSize, (7 - y) * squareSize);
					window.draw(pieceSprite);
				}
			}
		}

		// 单独画出正在拖拽的棋子，确保它在鼠标指针上，并且图层在最上面
		if (isDragging && draggedPiece != nullptr) {

			// 再次赋予纹理
			if (draggedPiece->getColor() == WHITE) {
				switch (draggedPiece->getType()) {
				case PAWN:   pieceSprite.setTexture(tex_wP); break;
				case ROOK:   pieceSprite.setTexture(tex_wR); break;
				case KNIGHT: pieceSprite.setTexture(tex_wN); break;
				case BISHOP: pieceSprite.setTexture(tex_wB); break;
				case QUEEN:  pieceSprite.setTexture(tex_wQ); break;
				case KING:   pieceSprite.setTexture(tex_wK); break;
				}
			}
			else {
				switch (draggedPiece->getType()) {
				case PAWN:   pieceSprite.setTexture(tex_bP); break;
				case ROOK:   pieceSprite.setTexture(tex_bR); break;
				case KNIGHT: pieceSprite.setTexture(tex_bN); break;
				case BISHOP: pieceSprite.setTexture(tex_bB); break;
				case QUEEN:  pieceSprite.setTexture(tex_bQ); break;
				case KING:   pieceSprite.setTexture(tex_bK); break;
				}
			}

			// 获取实时鼠标位置
			sf::Vector2i currentMousePos = sf::Mouse::getPosition(window);
			pieceSprite.setPosition(sf::Vector2f(currentMousePos.x, currentMousePos.y) - offset);
			window.draw(pieceSprite);
		}

		if (isGameOver) {
			// 1. 画一个全屏的黑色半透明遮罩，让后面的棋盘变暗
			sf::RectangleShape overlay(sf::Vector2f(800.f, 800.f));
			overlay.setFillColor(sf::Color(0, 0, 0, 150)); // 150代表透明度
			window.draw(overlay);

			// 2. 把文字设置到屏幕正中央
			gameOverText.setString(gameOverMessage);
			// 获取文字的实际宽高，用来计算中心点
			sf::FloatRect textRect = gameOverText.getLocalBounds();
			gameOverText.setOrigin(textRect.left + textRect.width / 2.0f,
				textRect.top + textRect.height / 2.0f);
			gameOverText.setPosition(sf::Vector2f(800.0f / 2.0f, 800.0f / 2.0f));

			// 3. 画出文字
			window.draw(gameOverText);
		}

		window.display();
	} // isOpen 循环结束

	return 0;
}