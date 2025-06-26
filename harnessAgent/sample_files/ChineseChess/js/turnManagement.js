// Turn Management Logic for Chinese Chess

class TurnManager {
    constructor() {
        this.currentPlayer = 'red'; // Red starts first
    }

    // Switch turns between red and black players
    switchTurn() {
        this.currentPlayer = this.currentPlayer === 'red' ? 'black' : 'red';
    }

    // Get the current player
    getCurrentPlayer() {
        return this.currentPlayer;
    }

    // Check if it's the given player's turn
    isPlayerTurn(player) {
        return this.currentPlayer === player;
    }
}

// Export the TurnManager class for use in game.js
export default TurnManager;