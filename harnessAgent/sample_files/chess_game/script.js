// Initialize the chessboard and game state
const chessboard = document.querySelector('.chess-board');
const moveHistory = document.querySelector('.move-history');
const controls = document.querySelector('.controls');
let selectedPiece = null;
let currentPlayer = 'white';
let gameState = initializeGame();

// Initialize the game with pieces in starting positions
function initializeGame() {
    return [
        ['♜', '♞', '♝', '♛', '♚', '♝', '♞', '♜'],
        ['♟', '♟', '♟', '♟', '♟', '♟', '♟', '♟'],
        ['', '', '', '', '', '', '', ''],
        ['', '', '', '', '', '', '', ''],
        ['', '', '', '', '', '', '', ''],
        ['', '', '', '', '', '', '', ''],
        ['♙', '♙', '♙', '♙', '♙', '♙', '♙', '♙'],
        ['♖', '♘', '♗', '♕', '♔', '♗', '♘', '♖']
    ];
}

// Render the chessboard based on the current game state
function renderChessboard() {
    chessboard.innerHTML = '';
    for (let row = 0; row < 8; row++) {
        for (let col = 0; col < 8; col++) {
            const square = document.createElement('div');
            square.className = `square ${(row + col) % 2 === 0 ? 'white' : 'black'}`;
            square.dataset.row = row;
            square.dataset.col = col;
            square.textContent = gameState[row][col];
            square.addEventListener('click', () => handleSquareClick(row, col));
            chessboard.appendChild(square);
        }
    }
}

// Handle square clicks (piece selection and movement)
function handleSquareClick(row, col) {
    const piece = gameState[row][col];
    if (!selectedPiece && piece && piece.startsWith(currentPlayer === 'white' ? '♙' : '♟')) {
        selectedPiece = { row, col };
        highlightValidMoves(row, col);
    } else if (selectedPiece) {
        movePiece(selectedPiece.row, selectedPiece.col, row, col);
        selectedPiece = null;
        clearHighlights();
    }
}

// Highlight valid moves for the selected piece
function highlightValidMoves(row, col) {
    // Simplified logic for pawn moves (expand for other pieces)
    const direction = currentPlayer === 'white' ? -1 : 1;
    const moves = [
        { row: row + direction, col }
    ];
    moves.forEach(move => {
        const square = document.querySelector(`[data-row="${move.row}"][data-col="${move.col}"]`);
        if (square) square.classList.add('highlight');
    });
}

// Clear highlighted squares
function clearHighlights() {
    document.querySelectorAll('.highlight').forEach(square => {
        square.classList.remove('highlight');
    });
}

// Move a piece and update the game state
function movePiece(fromRow, fromCol, toRow, toCol) {
    gameState[toRow][toCol] = gameState[fromRow][fromCol];
    gameState[fromRow][fromCol] = '';
    currentPlayer = currentPlayer === 'white' ? 'black' : 'white';
    renderChessboard();
    updateMoveHistory(fromRow, fromCol, toRow, toCol);
}

// Update the move history log
function updateMoveHistory(fromRow, fromCol, toRow, toCol) {
    const moveNotation = `${String.fromCharCode(97 + fromCol)}${8 - fromRow} → ${String.fromCharCode(97 + toCol)}${8 - toRow}`;
    const moveEntry = document.createElement('div');
    moveEntry.textContent = moveNotation;
    moveHistory.appendChild(moveEntry);
}

// Initialize the game
renderChessboard();

// Event listeners for player controls
document.querySelector('.new-game').addEventListener('click', () => {
    gameState = initializeGame();
    currentPlayer = 'white';
    renderChessboard();
    moveHistory.innerHTML = '';
});

document.querySelector('.undo').addEventListener('click', () => {
    // Implement undo logic (e.g., pop the last move from a history stack)
});

document.querySelector('.redo').addEventListener('click', () => {
    // Implement redo logic
});

// Navigation between screens
document.querySelector('.login-button').addEventListener('click', () => {
    window.location.href = 'login.html';
});

document.querySelector('.history-button').addEventListener('click', () => {
    window.location.href = 'history.html';
});

document.querySelector('.settings-button').addEventListener('click', () => {
    window.location.href = 'settings.html';
});