# AI Chess Engine - C++ & Raylib

A high-performance, fully compliant Chess Engine developed entirely from scratch in C++. 
This project bridges low-level C++ game logic with a custom-built Artificial Intelligence, all wrapped in a sleek, minimalist graphical user interface powered by Raylib.

## Core Features

- **Strict Chess Ruleset**: The engine goes beyond basic piece movement geometry to enforce full legal move validation. It actively evaluates board states to prevent pseudo-legal moves that would leave the King in check.
- **Advanced State Detection**: Automatically recognizes complex game-ending scenarios, perfectly differentiating between a Checkmate and a Stalemate to gracefully conclude the game.
- **Special Moves Integration**:
  - **Castling**: Fully supported for both Kingside and Queenside, dynamically checking castling rights, path clearance, and threatened squares.
  - **En Passant**: Accurately tracks pawn double-step history to allow valid diagonal captures on the subsequent turn.
  - **Pawn Promotion**: Pauses the game loop to present an interactive UI popup, allowing the player to safely promote a pawn to a Queen, Rook, Bishop, or Knight.
- **State Management (Undo)**: Features a robust state-rewind system. The "UNDO" button safely rolls back the move history, accurately restoring captured pieces and reverting both player and AI turns without memory leaks.

## Artificial Intelligence Architecture

The engine features a highly capable, custom-built AI opponent designed to challenge the player by calculating optimal future moves.

- **Minimax Algorithm**: The AI explores a deep game tree of potential future board states to minimize the player's advantage while maximizing its own.
- **Alpha-Beta Pruning**: Drastically optimizes the Minimax search. By dynamically pruning branches of the game tree that are proven to be worse than previously evaluated moves, the AI can search much deeper (Default Depth: 4 or 5) with near-instant response times.
- **Positional Heuristics**: Beyond simple material evaluation (e.g., valuing a Queen over a Knight), the AI utilizes detailed Piece-Square tables. This allows the AI to understand positional advantages, such as heavily favoring Knights in the center of the board and keeping the King safely tucked away during the early game.

## How to Play (Windows)

1. Ensure you have the `MinGW` C++ compiler installed on your machine.
2. The repository includes a `build_gui.bat` script that compiles the engine using the included Raylib source files.
3. Simply double-click `build_gui.bat` or run it from your terminal:
   ```cmd
   .\build_gui.bat
   ```
4. The game window will launch automatically upon successful compilation. 

## Controls
- **Move**: `Left-Click` a piece to select it (it will highlight in yellow), then click a valid destination square.
- **Undo**: Click the **UNDO** button in the bottom left corner at any time during your turn to rewind the previous move sequence.

---
*Built as a comprehensive showcase of C++ memory management, game loop architecture, and algorithmic AI implementation.*
