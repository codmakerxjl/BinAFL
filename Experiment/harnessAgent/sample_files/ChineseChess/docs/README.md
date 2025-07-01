# Chinese Chess Game Documentation

## Overview
This documentation provides a comprehensive guide to the Chinese Chess (Xiangqi) web application, covering its features, architecture, and usage.

## Project Structure
```
ChineseChess/
├── css/                # CSS files for styling
│   └── styles.css      # Main stylesheet
├── docs/               # Documentation
│   └── README.md       # This file
├── images/             # Image assets
├── js/                 # JavaScript files
│   ├── board.js        # Board representation
│   ├── game.js         # Main game logic
│   ├── gameLogic.js    # Move validation
│   ├── turnManagement.js # Turn management
│   └── aiOpponent.js   # AI opponent logic
├── index.html          # Main HTML file
└── game.js             # Entry point for game logic
```

## Features
1. **Board Representation**:
   - A 9x10 grid representing the Chinese Chess board.
   - Supports dynamic piece placement and movement.

2. **Piece Movement**:
   - Drag-and-drop functionality for intuitive gameplay.
   - Validates moves based on Chinese Chess rules.

3. **Turn Management**:
   - Alternates turns between red and black players.
   - Enforces turn-based gameplay.

4. **Game Over Conditions**:
   - Detects checkmate, stalemate, and draw scenarios.

5. **Multiplayer Support**:
   - Real-time gameplay using WebSockets.
   - Supports matchmaking and game sessions.

6. **AI Opponent**:
   - Implements minimax algorithm for decision-making.
   - Adjustable difficulty levels.

7. **Save/Load Game**:
   - Saves and loads game states for continuity.

8. **Sidebar Controls**:
   - Displays player stats, move history, and game controls.

## API Documentation (Server-Side)
### Endpoints
- `POST /api/game/create`: Create a new game session.
- `POST /api/game/join`: Join an existing game session.
- `POST /api/game/move`: Submit a move.
- `GET /api/game/state`: Fetch the current game state.

### WebSocket Events
- `gameStart`: Notifies players when the game starts.
- `move`: Broadcasts moves to both players.
- `gameOver`: Signals the end of the game.

## Usage
1. **Local Play**:
   - Open `index.html` in a browser to play against the AI or locally.

2. **Multiplayer**:
   - Start the server (`node server/app.js`).
   - Connect to the server via the client interface.

## Optimization
- **Performance**: Minimized DOM updates and optimized move validation.
- **Responsiveness**: Adaptive design for mobile and desktop.

## Future Enhancements
1. **Animations**: Smooth transitions for piece movement.
2. **Sound Effects**: Audio feedback for moves and captures.
3. **Local Storage**: Save game progress locally.

## License
This project is open-source under the MIT License.