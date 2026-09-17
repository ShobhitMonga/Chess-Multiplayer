@echo off
echo Compiling gui_chess.cpp with Raylib...
g++ gui_chess.cpp -o gui_chess.exe -Iinclude -Llib -lraylib -lopengl32 -lgdi32 -lwinmm
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful! Running gui_chess.exe...
gui_chess.exe
