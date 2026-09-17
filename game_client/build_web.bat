@echo off
echo Compiling gui_chess.cpp for WebAssembly with Emscripten...
echo Note: Ensure you have run 'emsdk_env.bat' so emcc is in your PATH.

em++ gui_chess.cpp -o index.html -Os -Wall -std=c++14 -D_DEFAULT_SOURCE -Wno-missing-braces ^
    -Iinclude -Llib_web -lraylib -s USE_GLFW=3 -s ASYNCIFY -lwebsocket.js -s EXPORTED_FUNCTIONS="['_main','_sendWebRTCMessage']" -s EXPORTED_RUNTIME_METHODS="['ccall']" --preload-file assets --shell-file shell.html

if %errorlevel% neq 0 (
    echo WebAssembly Compilation failed!
    pause
    exit /b %errorlevel%
)
echo Compilation successful! You can now serve this folder using a local web server (e.g. 'python -m http.server 8000') and open index.html in your browser.
pause
