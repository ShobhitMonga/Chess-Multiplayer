const { WebSocketServer } = require('ws');

const port = process.env.PORT || 8080;
const wss = new WebSocketServer({ port });

console.log(`WebSocket server running on port ${port}`);

let waitingPlayer = null;
let rooms = new Map();

wss.on('connection', (ws) => {
    console.log('New player connected.');

    if (waitingPlayer) {
        // We have a waiting player, let's pair them up!
        const player1 = waitingPlayer;
        const player2 = ws;
        
        const roomId = Date.now().toString();
        const room = { player1, player2 };
        rooms.set(player1, room);
        rooms.set(player2, room);
        
        waitingPlayer = null;

        // Assign colors
        player1.send(JSON.stringify({ type: 'start', color: 'White' }));
        player2.send(JSON.stringify({ type: 'start', color: 'Black' }));
        console.log(`Game started! Room ID: ${roomId}`);
    } else {
        // No one is waiting, so this player waits
        waitingPlayer = ws;
        ws.send(JSON.stringify({ type: 'wait', message: 'Waiting for an opponent...' }));
        console.log('Player added to waitlist.');
    }

    ws.on('message', (message) => {
        try {
            const data = JSON.parse(message);
            const room = rooms.get(ws);
            
            if (room) {
                // Relay move to the opponent
                const opponent = (ws === room.player1) ? room.player2 : room.player1;
                
                if (data.type === 'move') {
                    opponent.send(JSON.stringify({ type: 'move', data: data.data }));
                }
            }
        } catch (err) {
            console.error('Error parsing message:', err);
        }
    });

    ws.on('close', () => {
        console.log('Player disconnected.');
        if (waitingPlayer === ws) {
            waitingPlayer = null;
        } else {
            const room = rooms.get(ws);
            if (room) {
                // Notify opponent
                const opponent = (ws === room.player1) ? room.player2 : room.player1;
                if (opponent.readyState === opponent.OPEN) {
                    opponent.send(JSON.stringify({ type: 'disconnect', message: 'Opponent disconnected.' }));
                }
                rooms.delete(room.player1);
                rooms.delete(room.player2);
            }
        }
    });
});
