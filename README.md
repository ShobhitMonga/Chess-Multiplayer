 ♟️ AI Chess: Real-Time Multiplayer & Voice

A fully featured, real-time multiplayer chess game built from scratch in **C++** using **Raylib**. The game is compiled to **WebAssembly (WASM)**, allowing it to run natively in any modern web browser without any downloads or installations. 

It features instant multiplayer matchmaking and built-in **Peer-to-Peer Voice Chat**.

✨ Features

* **Native C++ Performance on the Web:** The core game logic and rendering engine are written entirely in C++ and Raylib, compiled to WebAssembly via Emscripten for lightning-fast browser performance.
* **Real-Time Multiplayer:** Play instantly against anyone in the world. Powered by a lightweight Node.js WebSocket signaling server.
* **In-Game Voice Chat:** Talk to your opponent in real-time! Uses WebRTC to establish a secure, low-latency Peer-to-Peer audio tunnel directly between browsers.

* **Cross-Platform:** Works on Windows, Mac, Linux, and Mobile browsers.

 🏗️ Architecture

This project elegantly bridges traditional desktop game development with modern web technologies:

1. **The Game Engine (Frontend):** 
   Written in C++ using the Raylib graphics library. Emscripten translates the C++ code into WebAssembly (`.wasm`), which is drawn onto an HTML5 Canvas (`shell.html`).
2. **The Post Office (Backend):** 
   A Node.js WebSocket server hosted on Render.com acts as a lightweight signaling server. It pairs players and acts as a router to trade chess moves (serialized as JSON strings) between the two C++ clients.
3. **The Walkie-Talkie (Voice Chat):** 
   Javascript captures the user's microphone and negotiates a WebRTC connection. A custom C++ to JS bridge (`Module.ccall`) passes the connection data (SDP/ICE candidates) through the WebSocket server to establish a direct Peer-to-Peer audio stream.

🚀 How to Play

1. Open the game link in your browser.
2. Click **VS Player** to enter the matchmaking queue.
3. Allow microphone access when prompted by the browser.
4. Have a friend open the link on their device and click **VS Player**.
5. You will instantly connect, the board will set up, and your voice chat will begin!

🛠️ Built With

* **C++ & Raylib** - Game logic and graphics rendering
* **Emscripten** - Compiling C++ to WebAssembly
* **Node.js & WebSockets (ws)** - Real-time multiplayer signaling server
* **WebRTC** - Peer-to-peer voice communications
