#include "raylib.h"
#include <iostream>
#include <cstdio>
#ifdef __EMSCRIPTEN__
#include <emscripten/websocket.h>
#include <emscripten.h>
#endif
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

using namespace std;

// --- Core Chess Logic ---

enum class PieceColor { White, Black };
class Piece;

struct Move {
    int fromRow, fromCol, toRow, toCol;
    Piece* capturedPiece;
    
    bool isCastling;
    bool isEnPassant;
    bool isPromotion;
    char promotionChar; 
    
    bool pieceHasMovedBefore;
    
    int rookFromRow, rookFromCol, rookToRow, rookToCol;
    bool rookHasMovedBefore;

    Move(int fr, int fc, int tr, int tc) {
        fromRow = fr; fromCol = fc; toRow = tr; toCol = tc;
        capturedPiece = nullptr;
        isCastling = false;
        isEnPassant = false;
        isPromotion = false;
        promotionChar = 'Q';
        pieceHasMovedBefore = false;
        rookFromRow = -1; rookFromCol = -1; rookToRow = -1; rookToCol = -1;
        rookHasMovedBefore = false;
    }
};

class Piece {
public:
    char symbol;
    PieceColor color;
    bool hasMoved;
    Piece(char sym, PieceColor col) : symbol(sym), color(col), hasMoved(false) {}
    virtual ~Piece() = default;
    virtual bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) = 0;
};

class Pawn : public Piece {
public:
    Pawn(PieceColor col) : Piece('P', col) {}
    bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) override {
        int direction = (color == PieceColor::White) ? -1 : 1; 
        if (fromCol == toCol) {
            if (toRow == fromRow + direction && board[toRow][toCol] == nullptr) return true;
            int startRow = (color == PieceColor::White) ? 6 : 1;
            if (fromRow == startRow && toRow == fromRow + (direction * 2) && 
                board[fromRow + direction][fromCol] == nullptr && board[toRow][toCol] == nullptr) return true;
        } else if (abs(toCol - fromCol) == 1 && toRow == fromRow + direction) {
            return true; 
        }
        return false;
    }
};

class Knight : public Piece {
public:
    Knight(PieceColor col) : Piece('N', col) {}
    bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) override {
        int rowDiff = abs(toRow - fromRow);
        int colDiff = abs(toCol - fromCol);
        return (rowDiff == 2 && colDiff == 1) || (rowDiff == 1 && colDiff == 2);
    }
};

class King : public Piece {
public:
    King(PieceColor col) : Piece('K', col) {}
    bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) override {
        int rowDiff = abs(toRow - fromRow);
        int colDiff = abs(toCol - fromCol);
        return (rowDiff <= 1 && colDiff <= 1) && !(rowDiff == 0 && colDiff == 0);
    }
};

class Rook : public Piece {
public:
    Rook(PieceColor col) : Piece('R', col) {}
    bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) override {
        if (fromRow != toRow && fromCol != toCol) return false;
        int rowDir = (toRow > fromRow) ? 1 : ((toRow < fromRow) ? -1 : 0);
        int colDir = (toCol > fromCol) ? 1 : ((toCol < fromCol) ? -1 : 0);
        int currentRow = fromRow + rowDir;
        int currentCol = fromCol + colDir;
        while (currentRow != toRow || currentCol != toCol) {
            if (board[currentRow][currentCol] != nullptr) return false; 
            currentRow += rowDir; currentCol += colDir;
        }
        return true;
    }
};

class Bishop : public Piece {
public:
    Bishop(PieceColor col) : Piece('B', col) {}
    bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) override {
        if (abs(toRow - fromRow) != abs(toCol - fromCol)) return false; 
        int rowDir = (toRow > fromRow) ? 1 : -1;
        int colDir = (toCol > fromCol) ? 1 : -1;
        int currentRow = fromRow + rowDir;
        int currentCol = fromCol + colDir;
        while (currentRow != toRow && currentCol != toCol) {
            if (board[currentRow][currentCol] != nullptr) return false; 
            currentRow += rowDir; currentCol += colDir;
        }
        return true;
    }
};

class Queen : public Piece {
public:
    Queen(PieceColor col) : Piece('Q', col) {}
    bool isValidMove(int fromRow, int fromCol, int toRow, int toCol, Piece* board[8][8]) override {
        bool straight = (fromRow == toRow || fromCol == toCol);
        bool diagonal = (abs(toRow - fromRow) == abs(toCol - fromCol));
        if (!straight && !diagonal) return false;
        int rowDir = (toRow > fromRow) ? 1 : ((toRow < fromRow) ? -1 : 0);
        int colDir = (toCol > fromCol) ? 1 : ((toCol < fromCol) ? -1 : 0);
        int currentRow = fromRow + rowDir;
        int currentCol = fromCol + colDir;
        while (currentRow != toRow || currentCol != toCol) {
            if (board[currentRow][currentCol] != nullptr) return false; 
            currentRow += rowDir; currentCol += colDir;
        }
        return true;
    }
};


enum class GameState { Ongoing, WhiteWins, BlackWins, Draw };

class ChessBoard {
public:
    Piece* board[8][8]; //  This is the literal chessboard in the computer's memory! It is a 2D grid. Because it holds pointers (Piece*), a square can either point to a piece (like a Knight) or be a nullptr (which means the square is completely empty).
    bool whiteTurn; // A simple true/false toggle. If true, White plays. If false, Black plays.
    GameState state;
    vector<Move> moveHistory;
    vector<Piece*> capturedPieces; // When a piece is captured, it is removed from the board[8][8] grid and placed in this vector or list.

    ChessBoard() {  //A Constructor is the code that automatically runs the exact moment a ChessBoard is created. First, it sweeps the entire 8x8 board and sets every single square to nullptr (empty).
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 8; ++j)
                board[i][j] = nullptr;
        reset();
    }

    void reset() {
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 8; ++j)
                if (board[i][j] != nullptr) {
                    delete board[i][j];
                    board[i][j] = nullptr;
                }
        for (Piece* p : capturedPieces) delete p;
        capturedPieces.clear();
        moveHistory.clear();

        board[0][0] = new Rook(PieceColor::Black); board[0][1] = new Knight(PieceColor::Black);
        board[0][2] = new Bishop(PieceColor::Black); board[0][3] = new Queen(PieceColor::Black);
        board[0][4] = new King(PieceColor::Black); board[0][5] = new Bishop(PieceColor::Black);
        board[0][6] = new Knight(PieceColor::Black); board[0][7] = new Rook(PieceColor::Black);
        for(int i = 0; i < 8; i++) board[1][i] = new Pawn(PieceColor::Black);

        board[7][0] = new Rook(PieceColor::White); board[7][1] = new Knight(PieceColor::White);
        board[7][2] = new Bishop(PieceColor::White); board[7][3] = new Queen(PieceColor::White);
        board[7][4] = new King(PieceColor::White); board[7][5] = new Bishop(PieceColor::White);
        board[7][6] = new Knight(PieceColor::White); board[7][7] = new Rook(PieceColor::White);
        for(int i = 0; i < 8; i++) board[6][i] = new Pawn(PieceColor::White);
        
        whiteTurn = true;
        state = GameState::Ongoing;
    }

    ~ChessBoard() {   // It automatically runs right before the ChessBoard is destroyed (like when you click the "X" to close the game window).In C++, because you used the keyword new to create the pieces, you are completely responsible for deleting them, otherwise your computer will run out of RAM (a Memory Leak).
        for (int i = 0; i < 8; ++i) // these two for loops scans the board and deletes every piece that survived the game.
            for (int j = 0; j < 8; ++j)
                if (board[i][j] != nullptr) delete board[i][j];
        for (Piece* p : capturedPieces) delete p;  // this loop scans the capturedPieces and deletes every piece that died during the game. This ensures your game perfectly cleans up its memory before closing!
    }

    bool isSquareAttacked(int r, int c, PieceColor attackerColor) {  //It scans the entire board to see if any enemy piece can legally capture whatever is standing on a specific square (r, c).The game uses this primarily to check if a King is in Check, or if a King is trying to walk into danger!
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                Piece* p = board[i][j];
                if (p != nullptr && p->color == attackerColor) {
                    if (p->symbol == 'P' || p->symbol == 'p') {
                        int dir = (attackerColor == PieceColor::White) ? -1 : 1;
                        if (r == i + dir && abs(c - j) == 1) return true;
                    } else {
                        if (p->isValidMove(i, j, r, c, board)) return true;
                    }
                }
            }
        }
        return false;  // means this square is safe from enemy attacks. The King can safely walk here without being captured.
    }

    bool isInCheck(PieceColor kingColor) {   // function scans the board to find a player's King, and then checks if any enemy pieces are currently in a position to capture it. If the King is under attack, it returns true (check); if it is safe, it returns false.
        int kingRow = -1, kingCol = -1;
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                if (board[i][j] != nullptr && board[i][j]->color == kingColor && 
                    (board[i][j]->symbol == 'K' || board[i][j]->symbol == 'k')) {
                    kingRow = i; kingCol = j;
                    break;
                }
            }
        }
        if (kingRow == -1) return false; // Should never happen unless testing
        PieceColor attackerColor = (kingColor == PieceColor::White) ? PieceColor::Black : PieceColor::White;
        return isSquareAttacked(kingRow, kingCol, attackerColor);
    }

    void makeMove(Move& move) {
        Piece* p = board[move.fromRow][move.fromCol];
        move.pieceHasMovedBefore = p->hasMoved;
        p->hasMoved = true;

        if (move.isEnPassant) {
            move.capturedPiece = board[move.fromRow][move.toCol];
            board[move.fromRow][move.toCol] = nullptr;
        } else {
            move.capturedPiece = board[move.toRow][move.toCol];
        }

        board[move.toRow][move.toCol] = p;
        board[move.fromRow][move.fromCol] = nullptr;

        if (move.isCastling) {
            Piece* rook = board[move.rookFromRow][move.rookFromCol];
            move.rookHasMovedBefore = rook->hasMoved;
            rook->hasMoved = true;
            board[move.rookToRow][move.rookToCol] = rook;
            board[move.rookFromRow][move.rookFromCol] = nullptr;
        }

        if (move.isPromotion) {
            // Memory leak fixed in undoMove logic or vector tracking for real moves
            if (move.promotionChar == 'Q') board[move.toRow][move.toCol] = new Queen(p->color);
            else if (move.promotionChar == 'R') board[move.toRow][move.toCol] = new Rook(p->color);
            else if (move.promotionChar == 'B') board[move.toRow][move.toCol] = new Bishop(p->color);
            else if (move.promotionChar == 'N') board[move.toRow][move.toCol] = new Knight(p->color);
        }

        whiteTurn = !whiteTurn;
    }

    void undoMove(const Move& move) {
        Piece* p = board[move.toRow][move.toCol];
        
        if (move.isPromotion) {
            delete p;
            p = new Pawn(whiteTurn ? PieceColor::Black : PieceColor::White); // Undo is reverse color
            board[move.toRow][move.toCol] = p;
        }

        p->hasMoved = move.pieceHasMovedBefore;
        board[move.fromRow][move.fromCol] = p;

        if (move.isEnPassant) {
            board[move.toRow][move.toCol] = nullptr;
            board[move.fromRow][move.toCol] = move.capturedPiece;
        } else {
            board[move.toRow][move.toCol] = move.capturedPiece;
        }

        if (move.isCastling) {
            Piece* rook = board[move.rookToRow][move.rookToCol];
            rook->hasMoved = move.rookHasMovedBefore;
            board[move.rookFromRow][move.rookFromCol] = rook;
            board[move.rookToRow][move.rookToCol] = nullptr;
        }

        whiteTurn = !whiteTurn;
    }

    void executeRealMove(const Move& move) {
        moveHistory.push_back(move);
        makeMove(moveHistory.back());
        if (moveHistory.back().capturedPiece) {
            capturedPieces.push_back(moveHistory.back().capturedPiece);
        }
        if (moveHistory.back().isPromotion) {
            capturedPieces.push_back(board[moveHistory.back().toRow][moveHistory.back().toCol]); 
        }
        
        // Update Game State
        PieceColor nextColor = whiteTurn ? PieceColor::White : PieceColor::Black;
        vector<Move> validMoves = generateAllLegalMoves(nextColor);
        if (validMoves.empty()) {
            if (isInCheck(nextColor)) {
                state = whiteTurn ? GameState::BlackWins : GameState::WhiteWins;
            } else {
                state = GameState::Draw;
            }
        }
    }

    void undoRealMove() {
        if (moveHistory.empty()) return;
        Move m = moveHistory.back();
        moveHistory.pop_back();
        
        if (m.isPromotion) {
            auto it = std::find(capturedPieces.begin(), capturedPieces.end(), board[m.toRow][m.toCol]);
            if (it != capturedPieces.end()) capturedPieces.erase(it);
        }
        
        undoMove(m);
        
        if (m.capturedPiece) {
            auto it = std::find(capturedPieces.begin(), capturedPieces.end(), m.capturedPiece);
            if (it != capturedPieces.end()) capturedPieces.erase(it);
        }
        
        state = GameState::Ongoing;
    }

    vector<Move> generateAllLegalMoves(PieceColor color) {
        vector<Move> moves;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                Piece* p = board[r][c];
                if (p != nullptr && p->color == color) {
                    
                    // Normal moves
                    for (int tr = 0; tr < 8; tr++) {
                        for (int tc = 0; tc < 8; tc++) {
                            if (board[tr][tc] != nullptr && board[tr][tc]->color == color) continue;
                            
                            if (p->symbol == 'P' || p->symbol == 'p') {
                                int dir = (color == PieceColor::White) ? -1 : 1;
                                if (tc == c && tr == r + dir && board[tr][tc] == nullptr) {
                                    // Forward 1
                                    Move m(r, c, tr, tc);
                                    if (tr == 0 || tr == 7) m.isPromotion = true;
                                    moves.push_back(m);
                                } else if (tc == c && r == ((color == PieceColor::White) ? 6 : 1) && tr == r + 2*dir && board[r+dir][tc] == nullptr && board[tr][tc] == nullptr) {
                                    // Forward 2
                                    moves.push_back(Move(r, c, tr, tc));
                                } else if (abs(tc - c) == 1 && tr == r + dir && board[tr][tc] != nullptr && board[tr][tc]->color != color) {
                                    // Capture
                                    Move m(r, c, tr, tc);
                                    if (tr == 0 || tr == 7) m.isPromotion = true;
                                    moves.push_back(m);
                                } else if (!moveHistory.empty() && abs(tc - c) == 1 && tr == r + dir) {
                                    // En Passant
                                    const Move& lm = moveHistory.back();
                                    if (lm.toRow == r && lm.toCol == tc && lm.fromRow == r + 2*dir) {
                                        Piece* epTarget = board[lm.toRow][lm.toCol];
                                        if (epTarget && (epTarget->symbol == 'P' || epTarget->symbol == 'p')) {
                                            Move m(r, c, tr, tc);
                                            m.isEnPassant = true;
                                            moves.push_back(m);
                                        }
                                    }
                                }
                                continue;
                            }
                            
                            if (p->isValidMove(r, c, tr, tc, board)) {
                                moves.push_back(Move(r, c, tr, tc));
                            }
                        }
                    }
                    
                    // Castling
                    if ((p->symbol == 'K' || p->symbol == 'k') && !p->hasMoved && !isInCheck(color)) {
                        PieceColor enemy = (color == PieceColor::White) ? PieceColor::Black : PieceColor::White;
                        // Kingside
                        if (board[r][7] != nullptr && (board[r][7]->symbol == 'R' || board[r][7]->symbol == 'r') && !board[r][7]->hasMoved) {
                            if (board[r][5] == nullptr && board[r][6] == nullptr) {
                                if (!isSquareAttacked(r, 5, enemy) && !isSquareAttacked(r, 6, enemy)) {
                                    Move m(r, c, r, c + 2);
                                    m.isCastling = true;
                                    m.rookFromRow = r; m.rookFromCol = 7;
                                    m.rookToRow = r; m.rookToCol = 5;
                                    moves.push_back(m);
                                }
                            }
                        }
                        // Queenside
                        if (board[r][0] != nullptr && (board[r][0]->symbol == 'R' || board[r][0]->symbol == 'r') && !board[r][0]->hasMoved) {
                            if (board[r][1] == nullptr && board[r][2] == nullptr && board[r][3] == nullptr) {
                                if (!isSquareAttacked(r, 2, enemy) && !isSquareAttacked(r, 3, enemy)) {
                                    Move m(r, c, r, c - 2);
                                    m.isCastling = true;
                                    m.rookFromRow = r; m.rookFromCol = 0;
                                    m.rookToRow = r; m.rookToCol = 3;
                                    moves.push_back(m);
                                }
                            }
                        }
                    }
                }
            }
        }
        
        vector<Move> validMoves;
        for (Move& m : moves) {
            if (m.isPromotion) {
                char proms[] = {'Q', 'R', 'B', 'N'};
                for (char prm : proms) {
                    Move pm = m;
                    pm.promotionChar = prm;
                    makeMove(pm);
                    if (!isInCheck(color)) validMoves.push_back(pm);
                    undoMove(pm);
                }
            } else {
                makeMove(m);
                if (!isInCheck(color)) validMoves.push_back(m);
                undoMove(m);
            }
        }
        
        std::sort(validMoves.begin(), validMoves.end(), [this](const Move& a, const Move& b) {
            int scoreA = (board[a.toRow][a.toCol] != nullptr) ? 1 : 0;
            int scoreB = (board[b.toRow][b.toCol] != nullptr) ? 1 : 0;
            if (a.isPromotion) scoreA += 5;
            if (b.isPromotion) scoreB += 5;
            return scoreA > scoreB;
        });
        
        return validMoves;
    }

    const int centerControl[8][8] = {
        { -2, -1, -1, -1, -1, -1, -1, -2 },
        { -1,  0,  0,  0,  0,  0,  0, -1 },
        { -1,  0,  1,  1,  1,  1,  0, -1 },
        { -1,  0,  1,  2,  2,  1,  0, -1 },
        { -1,  0,  1,  2,  2,  1,  0, -1 },
        { -1,  0,  1,  1,  1,  1,  0, -1 },
        { -1,  0,  0,  0,  0,  0,  0, -1 },
        { -2, -1, -1, -1, -1, -1, -1, -2 }
    };

    int evaluateBoard() {
        int whiteMaterial = 0;
        int blackMaterial = 0;
        int whiteKingRow = -1, whiteKingCol = -1;
        int blackKingRow = -1, blackKingCol = -1;
        
        int score = 0;
        
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                Piece* p = board[r][c];
                if (p != nullptr) {
                    int val = 0;
                    if (p->symbol == 'P' || p->symbol == 'p') {
                        val = 100;
                        if (p->color == PieceColor::White) val += (7 - r) * 2; 
                        else val += r * 2; 
                    }
                    else if (p->symbol == 'N' || p->symbol == 'n') { val = 300 + centerControl[r][c] * 10; if(p->color == PieceColor::White) whiteMaterial += 300; else blackMaterial += 300; }
                    else if (p->symbol == 'B' || p->symbol == 'b') { val = 300 + centerControl[r][c] * 5; if(p->color == PieceColor::White) whiteMaterial += 300; else blackMaterial += 300; }
                    else if (p->symbol == 'R' || p->symbol == 'r') { val = 500; if(p->color == PieceColor::White) whiteMaterial += 500; else blackMaterial += 500; }
                    else if (p->symbol == 'Q' || p->symbol == 'q') { val = 900; if(p->color == PieceColor::White) whiteMaterial += 900; else blackMaterial += 900; }
                    else if (p->symbol == 'K' || p->symbol == 'k') { 
                        val = 9000; 
                        if (p->color == PieceColor::White) { whiteKingRow = r; whiteKingCol = c; }
                        else { blackKingRow = r; blackKingCol = c; }
                    }
                    
                    if (p->color == PieceColor::White) score += val;
                    else score -= val;
                }
            }
        }
        
        // Endgame Heuristics
        // If material is low OR one side has a huge advantage, hunt the opponent's king!
        int totalMaterial = whiteMaterial + blackMaterial;
        if (totalMaterial < 1500 || abs(score) > 600) {
            int endgameScore = 0;
            
            // Function to evaluate king being pushed to edge
            auto kingEdgeScore = [](int r, int c) {
                int distCenterR = std::max(3 - r, r - 4);
                int distCenterC = std::max(3 - c, c - 4);
                return (distCenterR + distCenterC) * 10;
            };
            
            // Function to evaluate distance between kings
            auto kingDistScore = [](int wr, int wc, int br, int bc) {
                return (abs(wr - br) + abs(wc - bc)) * 5; 
            };
            
            if (score > 600) { 
                // White is winning, drive black king to edge
                endgameScore += kingEdgeScore(blackKingRow, blackKingCol);
                endgameScore -= kingDistScore(whiteKingRow, whiteKingCol, blackKingRow, blackKingCol);
            } else if (score < -600) {
                // Black is winning, drive white king to edge
                endgameScore -= kingEdgeScore(whiteKingRow, whiteKingCol);
                endgameScore += kingDistScore(whiteKingRow, whiteKingCol, blackKingRow, blackKingCol);
            }
            
            // Weight the endgame score based on advantage, so it kicks in fully when up a lot of material
            score += endgameScore;
        }
        
        return score;
    }

    int minimax(int depth, int alpha, int beta, bool isMaximizing) {
        if (depth == 0) return evaluateBoard();
        PieceColor currentColor = isMaximizing ? PieceColor::White : PieceColor::Black;
        vector<Move> legalMoves = generateAllLegalMoves(currentColor);
        if (legalMoves.empty()) {
            if (isInCheck(currentColor)) return isMaximizing ? -9999 : 9999;
            else return 0;
        }

        if (isMaximizing) {
            int maxEval = -99999;
            for (Move& move : legalMoves) {
                makeMove(move);
                int eval = minimax(depth - 1, alpha, beta, false);
                undoMove(move);
                maxEval = max(maxEval, eval);
                alpha = max(alpha, eval);
                if (beta <= alpha) break; 
            }
            return maxEval;
        } else {
            int minEval = 99999;
            for (Move& move : legalMoves) {
                makeMove(move);
                int eval = minimax(depth - 1, alpha, beta, true);
                undoMove(move);
                minEval = min(minEval, eval);
                beta = min(beta, eval);
                if (beta <= alpha) break; 
            }
            return minEval;
        }
    }

    Move getBestMove(int depth, PieceColor aiColor) {
        vector<Move> legalMoves = generateAllLegalMoves(aiColor);
        if (legalMoves.empty()) return Move(0,0,0,0); 
        Move bestMove = legalMoves[0];
        if (aiColor == PieceColor::White) {
            int bestVal = -99999;
            for (Move& move : legalMoves) {
                makeMove(move);
                int moveVal = minimax(depth - 1, -99999, 99999, false);
                undoMove(move);
                if (moveVal > bestVal) {
                    bestVal = moveVal;
                    bestMove = move;
                }
            }
        } else {
            int bestVal = 99999;
            for (Move& move : legalMoves) {
                makeMove(move);
                int moveVal = minimax(depth - 1, -99999, 99999, true);
                undoMove(move);
                if (moveVal < bestVal) {
                    bestVal = moveVal;
                    bestMove = move;
                }
            }
        }
        return bestMove;
    }
    
    Move* attemptHumanMove(int fromRow, int fromCol, int toRow, int toCol, char promotionChar = 'Q') {
        if (state != GameState::Ongoing) return nullptr;
        vector<Move> moves = generateAllLegalMoves(whiteTurn ? PieceColor::White : PieceColor::Black);
        for (Move& m : moves) {
            if (m.fromRow == fromRow && m.fromCol == fromCol && m.toRow == toRow && m.toCol == toCol) {
                if (m.isPromotion) {
                    if (m.promotionChar == promotionChar) {
                        return new Move(m);
                    }
                } else {
                    return new Move(m);
                }
            }
        }
        return nullptr;
    }
};


enum class AppMode { MENU, VS_AI, VS_PLAYER_WAITING, VS_PLAYER_PLAYING };

// --- Multiplayer State ---
struct GlobalState {
    ChessBoard* game;
    AppMode* appMode;
    bool isWhitePlayer = true;
    bool isMyTurn = true;
    bool isConnected = false;
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_WEBSOCKET_T ws;
#endif
};

GlobalState* globalStatePtr = nullptr;

extern "C" {
    void sendWebRTCMessage(const char* message) {
#ifdef __EMSCRIPTEN__
        if (globalStatePtr && globalStatePtr->isConnected) {
            emscripten_websocket_send_utf8_text(globalStatePtr->ws, message);
        }
#endif
    }
}

#ifdef __EMSCRIPTEN__
EM_BOOL onopen_cb(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData) {
    GlobalState* g = (GlobalState*)userData;
    g->isConnected = true;
    return EM_TRUE;
}

EM_BOOL onclose_cb(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData) {
    GlobalState* g = (GlobalState*)userData;
    *(g->appMode) = AppMode::MENU;
    g->isConnected = false;
    return EM_TRUE;
}

EM_BOOL onmessage_cb(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData) {
    GlobalState* g = (GlobalState*)userData;
    std::string msg((const char*)websocketEvent->data, websocketEvent->numBytes);
    
    if (msg.find("\"type\":\"start\"") != std::string::npos) {
        if (msg.find("\"color\":\"White\"") != std::string::npos) {
            g->isWhitePlayer = true;
            g->isMyTurn = true; // White moves first
        } else {
            g->isWhitePlayer = false;
            g->isMyTurn = false; // Black waits
        }
        *(g->appMode) = AppMode::VS_PLAYER_PLAYING;
        EM_ASM({
            if (window.startWebRTC) {
                window.startWebRTC($0);
            }
        }, g->isWhitePlayer ? 1 : 0);
    } else if (msg.find("\"type\":\"move\"") != std::string::npos) {
        size_t pos = msg.find("\"data\":\"");
        if (pos != std::string::npos) {
            std::string m = msg.substr(pos + 8, 9); // e.g. "6,4,4,4,Q"
            int fr = m[0] - '0';
            int fc = m[2] - '0';
            int tr = m[4] - '0';
            int tc = m[6] - '0';
            char promo = m[8];
            
            Move* oppMove = g->game->attemptHumanMove(fr, fc, tr, tc, promo);
            if (oppMove) {
                g->game->executeRealMove(*oppMove);
                delete oppMove;
            }
            g->isMyTurn = true;
        }
    } else if (msg.find("\"type\":\"webrtc\"") != std::string::npos) {
        EM_ASM({
            if (window.handleWebRTCMessage) {
                window.handleWebRTCMessage(UTF8ToString($0));
            }
        }, msg.c_str());
    } else if (msg.find("\"type\":\"disconnect\"") != std::string::npos) {
        *(g->appMode) = AppMode::MENU; // Opponent left, go back to menu
    }
    
    return EM_TRUE;
}
#endif

// --- GUI Implementation using Raylib ---

int main() {
    const int screenWidth = 800;
    const int screenHeight = 850;
    const int tileSize = 100;

    InitWindow(screenWidth, screenHeight, "AI Chess - C++ GUI");
    SetTargetFPS(60);
    
    Texture2D texWK = LoadTexture("assets/wk.png");
    Texture2D texWQ = LoadTexture("assets/wq.png");
    Texture2D texWR = LoadTexture("assets/wr.png");
    Texture2D texWB = LoadTexture("assets/wb.png");
    Texture2D texWN = LoadTexture("assets/wn.png");
    Texture2D texWP = LoadTexture("assets/wp.png");
    Texture2D texBK = LoadTexture("assets/bk.png");
    Texture2D texBQ = LoadTexture("assets/bq.png");
    Texture2D texBR = LoadTexture("assets/br.png");
    Texture2D texBB = LoadTexture("assets/bb.png");
    Texture2D texBN = LoadTexture("assets/bn.png");
    Texture2D texBP = LoadTexture("assets/bp.png");

    ChessBoard game;
    int aiDepth = 4; // Using depth 4 for decent play
    
    int selectedRow = -1;
    int selectedCol = -1;
    
    bool aiThinking = false;
    
    bool awaitingPromotion = false;
    int promFromRow, promFromCol, promToRow, promToCol;
    
    Color darkSquare = { 181, 136, 99, 255 }; // Walnut Wood
    Color lightSquare = { 240, 217, 181, 255 }; // Maple Wood
    Color highlightColor = { 205, 210, 106, 200 }; // Yellow-green highlight
    Color selectedColor = { 205, 210, 106, 255 }; // Solid highlight for selected
    
    AppMode appMode = AppMode::MENU;
    GlobalState gState;
    gState.game = &game;
    gState.appMode = &appMode;
    globalStatePtr = &gState;

    auto drawPiece = [&](Piece* p, int r, int c) {
        Texture2D tex = texWK; 
        if (p->color == PieceColor::White) {
            if (p->symbol == 'K') tex = texWK;
            else if (p->symbol == 'Q') tex = texWQ;
            else if (p->symbol == 'R') tex = texWR;
            else if (p->symbol == 'B') tex = texWB;
            else if (p->symbol == 'N') tex = texWN;
            else if (p->symbol == 'P' || p->symbol == 'p') tex = texWP;
        } else {
            if (p->symbol == 'K' || p->symbol == 'k') tex = texBK;
            else if (p->symbol == 'Q' || p->symbol == 'q') tex = texBQ;
            else if (p->symbol == 'R' || p->symbol == 'r') tex = texBR;
            else if (p->symbol == 'B' || p->symbol == 'b') tex = texBB;
            else if (p->symbol == 'N' || p->symbol == 'n') tex = texBN;
            else if (p->symbol == 'P' || p->symbol == 'p') tex = texBP;
        }
        float scale = (float)tileSize / tex.width;
        Vector2 pos = {(float)c * tileSize, (float)r * tileSize};
        DrawTextureEx(tex, pos, 0.0f, scale, WHITE);
    };

    while (!WindowShouldClose()) {

        if (appMode == AppMode::MENU) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("CHESS", screenWidth/2 - MeasureText("CHESS", 60)/2, 200, 60, BLACK);
            
            Rectangle btnAI = { (float)(screenWidth/2 - 150), 400, 300, 60 };
            Rectangle btnMulti = { (float)(screenWidth/2 - 150), 500, 300, 60 };
            
            DrawRectangleRec(btnAI, LIGHTGRAY);
            DrawRectangleLinesEx(btnAI, 2, BLACK);
            DrawText("VS Computer", btnAI.x + 150 - MeasureText("VS Computer", 30)/2, btnAI.y + 15, 30, BLACK);
            
            DrawRectangleRec(btnMulti, LIGHTGRAY);
            DrawRectangleLinesEx(btnMulti, 2, BLACK);
            DrawText("VS Player", btnMulti.x + 150 - MeasureText("VS Player", 30)/2, btnMulti.y + 15, 30, BLACK);
            
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mousePos = GetMousePosition();
                if (CheckCollisionPointRec(mousePos, btnAI)) {
                    appMode = AppMode::VS_AI;
                    game.reset();
                } else if (CheckCollisionPointRec(mousePos, btnMulti)) {
                    appMode = AppMode::VS_PLAYER_WAITING;
                    game.reset();
#ifdef __EMSCRIPTEN__
                    if (emscripten_websocket_is_supported()) {
                        EmscriptenWebSocketCreateAttributes ws_attrs = {
                            "wss://chess-multiplayer-4xgy.onrender.com", 
                            NULL,
                            EM_TRUE
                        };
                        gState.ws = emscripten_websocket_new(&ws_attrs);
                        emscripten_websocket_set_onopen_callback(gState.ws, &gState, onopen_cb);
                        emscripten_websocket_set_onmessage_callback(gState.ws, &gState, onmessage_cb);
                        emscripten_websocket_set_onclose_callback(gState.ws, &gState, onclose_cb);
                    }
#else
                    appMode = AppMode::MENU;
#endif
                }
            }
            EndDrawing();
            continue; 
        }

        if (appMode == AppMode::VS_PLAYER_WAITING) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            DrawText("Connecting to server...", screenWidth/2 - 150, 400, 30, BLACK);
            EndDrawing();
            continue;
        }
        
        // AI Turn Logic (only in VS_AI mode)
        if (appMode == AppMode::VS_AI && game.state == GameState::Ongoing && !game.whiteTurn && !aiThinking && !awaitingPromotion) {
            aiThinking = true;
        }

        if (aiThinking) {
            BeginDrawing();
            ClearBackground(RAYWHITE);
            bool isFlipped = (appMode == AppMode::VS_PLAYER_PLAYING && !gState.isWhitePlayer);
            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    int drawR = isFlipped ? 7 - r : r;
                    int drawC = isFlipped ? 7 - c : c;
                    Color tileColor = ((r + c) % 2 == 0) ? lightSquare : darkSquare;
                    DrawRectangle(drawC * tileSize, drawR * tileSize, tileSize, tileSize, tileColor);
                    Piece* p = game.board[r][c];
                    if (p != nullptr) drawPiece(p, drawR, drawC);
                }
            }
            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.3f));
            DrawText("AI is thinking...", screenWidth/2 - 120, screenHeight/2 - 20, 30, WHITE);
            EndDrawing();
            
            Move aiMove = game.getBestMove(aiDepth, PieceColor::Black);
            game.executeRealMove(aiMove);
            aiThinking = false;
        }

        // Undo Button Logic
        if (appMode == AppMode::VS_AI && !aiThinking && !awaitingPromotion) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mousePos = GetMousePosition();
                if (mousePos.y >= 800 && mousePos.y <= 850 && mousePos.x >= 0 && mousePos.x <= 200) {
                    if (game.moveHistory.size() >= 2) {
                        game.undoRealMove();
                        game.undoRealMove();
                        selectedRow = -1; selectedCol = -1;
                    } else if (game.moveHistory.size() == 1) {
                        game.undoRealMove();
                        selectedRow = -1; selectedCol = -1;
                    }
                }
            }
        }

        // Back to Menu Button Logic
        if (!aiThinking && !awaitingPromotion) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mousePos = GetMousePosition();
                if (mousePos.y >= 800 && mousePos.y <= 850 && mousePos.x >= screenWidth - 200 && mousePos.x <= screenWidth) {
                    appMode = AppMode::MENU;
#ifdef __EMSCRIPTEN__
                    if (gState.isConnected) {
                        emscripten_websocket_close(gState.ws, 1000, "User quit");
                        gState.isConnected = false;
                    }
#endif
                }
            }
        }

        // Human Turn Logic
        bool myTurn = (appMode == AppMode::VS_AI && game.whiteTurn) || (appMode == AppMode::VS_PLAYER_PLAYING && gState.isMyTurn);
        
        if (game.state == GameState::Ongoing && myTurn && !aiThinking) {
            if (awaitingPromotion) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    int uiX = screenWidth/2 - 200;
                    int uiY = screenHeight/2 - 50;
                    if (mousePos.y >= uiY && mousePos.y <= uiY + 100) {
                        char choice = ' ';
                        if (mousePos.x >= uiX && mousePos.x < uiX + 100) choice = 'Q';
                        else if (mousePos.x >= uiX + 100 && mousePos.x < uiX + 200) choice = 'R';
                        else if (mousePos.x >= uiX + 200 && mousePos.x < uiX + 300) choice = 'B';
                        else if (mousePos.x >= uiX + 300 && mousePos.x < uiX + 400) choice = 'N';
                        
                        if (choice != ' ') {
                            Move* hm = game.attemptHumanMove(promFromRow, promFromCol, promToRow, promToCol, choice);
                            if (hm) {
                                game.executeRealMove(*hm);
#ifdef __EMSCRIPTEN__
                                if (appMode == AppMode::VS_PLAYER_PLAYING) {
                                    char buf[128];
                                    sprintf(buf, "{\"type\":\"move\",\"data\":\"%d,%d,%d,%d,%c\"}", hm->fromRow, hm->fromCol, hm->toRow, hm->toCol, choice);
                                    emscripten_websocket_send_utf8_text(gState.ws, buf);
                                    gState.isMyTurn = false;
                                }
#endif
                                delete hm;
                            }
                            awaitingPromotion = false;
                        }
                    }
                }
            } else {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                    Vector2 mousePos = GetMousePosition();
                    bool isFlipped = (appMode == AppMode::VS_PLAYER_PLAYING && !gState.isWhitePlayer);
                    int col = mousePos.x / tileSize;
                    int row = mousePos.y / tileSize;
                    if (isFlipped) {
                        col = 7 - col;
                        row = 7 - row;
                    }
                    
                    if (row < 8) { 
                        PieceColor myColor = (appMode == AppMode::VS_PLAYER_PLAYING) ? (gState.isWhitePlayer ? PieceColor::White : PieceColor::Black) : PieceColor::White;

                        if (selectedRow == -1 && selectedCol == -1) {
                            if (game.board[row][col] != nullptr && game.board[row][col]->color == myColor) {
                                selectedRow = row;
                                selectedCol = col;
                            }
                        } else {
                            if (selectedRow == row && selectedCol == col) {
                                selectedRow = -1;
                                selectedCol = -1;
                            } else {
                                vector<Move> validMoves = game.generateAllLegalMoves(myColor);
                                bool isPromoMove = false;
                                bool foundMove = false;
                                for (Move& m : validMoves) {
                                    if (m.fromRow == selectedRow && m.fromCol == selectedCol && m.toRow == row && m.toCol == col) {
                                        foundMove = true;
                                        if (m.isPromotion) isPromoMove = true;
                                        break;
                                    }
                                }

                                if (foundMove) {
                                    if (isPromoMove) {
                                        awaitingPromotion = true;
                                        promFromRow = selectedRow; promFromCol = selectedCol;
                                        promToRow = row; promToCol = col;
                                    } else {
                                        Move* hm = game.attemptHumanMove(selectedRow, selectedCol, row, col);
                                        if (hm) {
                                            game.executeRealMove(*hm);
#ifdef __EMSCRIPTEN__
                                            if (appMode == AppMode::VS_PLAYER_PLAYING) {
                                                char buf[128];
                                                sprintf(buf, "{\"type\":\"move\",\"data\":\"%d,%d,%d,%d, \"}", hm->fromRow, hm->fromCol, hm->toRow, hm->toCol);
                                                emscripten_websocket_send_utf8_text(gState.ws, buf);
                                                gState.isMyTurn = false;
                                            }
#endif
                                            delete hm;
                                        }
                                    }
                                    selectedRow = -1;
                                    selectedCol = -1;
                                } else {
                                    if (game.board[row][col] != nullptr && game.board[row][col]->color == myColor) {
                                        selectedRow = row;
                                        selectedCol = col;
                                    } else {
                                        selectedRow = -1;
                                        selectedCol = -1;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Render
        if (!aiThinking) {
            BeginDrawing();
            ClearBackground(RAYWHITE);

            bool isFlipped = (appMode == AppMode::VS_PLAYER_PLAYING && !gState.isWhitePlayer);

            for (int r = 0; r < 8; r++) {
                for (int c = 0; c < 8; c++) {
                    int drawR = isFlipped ? 7 - r : r;
                    int drawC = isFlipped ? 7 - c : c;
                    Color tileColor = ((r + c) % 2 == 0) ? lightSquare : darkSquare;
                    DrawRectangle(drawC * tileSize, drawR * tileSize, tileSize, tileSize, tileColor);
                    
                    if (r == selectedRow && c == selectedCol) {
                        DrawRectangle(drawC * tileSize, drawR * tileSize, tileSize, tileSize, selectedColor);
                    }
                    
                    Piece* p = game.board[r][c];
                    if (p != nullptr) drawPiece(p, drawR, drawC);
                }
            }
            
            if (!game.moveHistory.empty()) {
                int fr = game.moveHistory.back().fromRow;
                int fc = game.moveHistory.back().fromCol;
                int tr = game.moveHistory.back().toRow;
                int tc = game.moveHistory.back().toCol;
                if (isFlipped) { fr = 7 - fr; fc = 7 - fc; tr = 7 - tr; tc = 7 - tc; }
                DrawRectangleLines(fc * tileSize, fr * tileSize, tileSize, tileSize, highlightColor);
                DrawRectangleLines(tc * tileSize, tr * tileSize, tileSize, tileSize, highlightColor);
            }
            
            if (appMode == AppMode::VS_AI) {
                DrawRectangle(0, 800, 200, 50, LIGHTGRAY);
                DrawRectangleLines(0, 800, 200, 50, DARKGRAY);
                DrawText("UNDO", 65, 815, 20, BLACK);
            } else if (appMode == AppMode::VS_PLAYER_PLAYING) {
                DrawText(gState.isWhitePlayer ? "You are WHITE" : "You are BLACK", 20, 815, 20, BLACK);
                if (gState.isMyTurn) DrawText("YOUR TURN", 300, 815, 20, DARKGREEN);
                else DrawText("WAITING FOR OPPONENT...", 250, 815, 20, MAROON);
            }

            DrawRectangle(screenWidth - 200, 800, 200, 50, LIGHTGRAY);
            DrawRectangleLines(screenWidth - 200, 800, 200, 50, DARKGRAY);
            DrawText("MENU", screenWidth - 135, 815, 20, BLACK);

            if (awaitingPromotion) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.5f));
                int uiX = screenWidth/2 - 200;
                int uiY = screenHeight/2 - 50;
                DrawRectangle(uiX, uiY, 400, 100, RAYWHITE);
                DrawRectangleLines(uiX, uiY, 400, 100, BLACK);
                
                float scale = 100.0f / texWQ.width;
                DrawTextureEx(texWQ, {(float)uiX, (float)uiY}, 0.0f, scale, WHITE);
                DrawTextureEx(texWR, {(float)uiX + 100, (float)uiY}, 0.0f, scale, WHITE);
                DrawTextureEx(texWB, {(float)uiX + 200, (float)uiY}, 0.0f, scale, WHITE);
                DrawTextureEx(texWN, {(float)uiX + 300, (float)uiY}, 0.0f, scale, WHITE);
            }

            if (game.state != GameState::Ongoing) {
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(BLACK, 0.6f));
                const char* msg = "";
                if (game.state == GameState::WhiteWins) msg = "Checkmate! White Wins!";
                else if (game.state == GameState::BlackWins) msg = "Checkmate! Black Wins!";
                else if (game.state == GameState::Draw) msg = "Stalemate! It's a Draw!";
                
                int len = MeasureText(msg, 40);
                DrawText(msg, screenWidth/2 - len/2, screenHeight/2 - 20, 40, WHITE);
            }

            EndDrawing();
        }
    }
    
    UnloadTexture(texWK); UnloadTexture(texWQ); UnloadTexture(texWR); 
    UnloadTexture(texWB); UnloadTexture(texWN); UnloadTexture(texWP);
    UnloadTexture(texBK); UnloadTexture(texBQ); UnloadTexture(texBR); 
    UnloadTexture(texBB); UnloadTexture(texBN); UnloadTexture(texBP);

    CloseWindow();
    return 0;
}
