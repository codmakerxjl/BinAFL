// Initialize the game state
const gameState = {
    currentPlayer: 'red', // red starts first
    board: [], // 2D array representing the board
    pieces: [], // Array of piece objects
    moveHistory: [], // Log of moves for undo functionality
};

// Initialize the board and pieces
function initializeGame() {
    // Create a 9x10 grid for the board
    gameState.board = Array(10).fill().map(() => Array(9).fill(null));

    // Define the initial positions of the pieces
    gameState.pieces = [
        // Red pieces
        { type: 'rook', color: 'red', row: 0, col: 0 },
        { type: 'horse', color: 'red', row: 0, col: 1 },
        { type: 'elephant', color: 'red', row: 0, col: 2 },
        { type: 'advisor', color: 'red', row: 0, col: 3 },
        { type: 'general', color: 'red', row: 0, col: 4 },
        { type: 'advisor', color: 'red', row: 0, col: 5 },
        { type: 'elephant', color: 'red', row: 0, col: 6 },
        { type: 'horse', color: 'red', row: 0, col: 7 },
        { type: 'rook', color: 'red', row: 0, col: 8 },
        { type: 'cannon', color: 'red', row: 2, col: 1 },
        { type: 'cannon', color: 'red', row: 2, col: 7 },
        { type: 'soldier', color: 'red', row: 3, col: 0 },
        { type: 'soldier', color: 'red', row: 3, col: 2 },
        { type: 'soldier', color: 'red', row: 3, col: 4 },
        { type: 'soldier', color: 'red', row: 3, col: 6 },
        { type: 'soldier', color: 'red', row: 3, col: 8 },

        // Black pieces
        { type: 'rook', color: 'black', row: 9, col: 0 },
        { type: 'horse', color: 'black', row: 9, col: 1 },
        { type: 'elephant', color: 'black', row: 9, col: 2 },
        { type: 'advisor', color: 'black', row: 9, col: 3 },
        { type: 'general', color: 'black', row: 9, col: 4 },
        { type: 'advisor', color: 'black', row: 9, col: 5 },
        { type: 'elephant', color: 'black', row: 9, col: 6 },
        { type: 'horse', color: 'black', row: 9, col: 7 },
        { type: 'rook', color: 'black', row: 9, col: 8 },
        { type: 'cannon', color: 'black', row: 7, col: 1 },
        { type: 'cannon', color: 'black', row: 7, col: 7 },
        { type: 'soldier', color: 'black', row: 6, col: 0 },
        { type: 'soldier', color: 'black', row: 6, col: 2 },
        { type: 'soldier', color: 'black', row: 6, col: 4 },
        { type: 'soldier', color: 'black', row: 6, col: 6 },
        { type: 'soldier', color: 'black', row: 6, col: 8 },
    ];

    // Place pieces on the board
    gameState.pieces.forEach(piece => {
        gameState.board[piece.row][piece.col] = piece;
    });

    // Render the board
    renderBoard();

    // Update the turn indicator
    updateTurnIndicator();
}

// Render the board and pieces
function renderBoard() {
    const boardElement = document.querySelector('.board');
    boardElement.innerHTML = '';

    for (let row = 0; row < 10; row++) {
        for (let col = 0; col < 9; col++) {
            const cell = document.createElement('div');
            cell.className = 'cell';
            cell.dataset.row = row;
            cell.dataset.col = col;

            // Add river or palace class if applicable
            if (row === 4 || row === 5) {
                cell.classList.add('river');
            } else if ((row === 0 && col === 4) || (row === 9 && col === 4)) {
                cell.classList.add('palace');
            }

            // Place the piece if it exists
            const piece = gameState.board[row][col];
            if (piece) {
                const pieceElement = document.createElement('div');
                pieceElement.className = `piece ${piece.color}-piece`;
                pieceElement.textContent = getPieceSymbol(piece.type);
                pieceElement.draggable = true;
                pieceElement.dataset.row = row;
                pieceElement.dataset.col = col;
                pieceElement.addEventListener('dragstart', dragStart);
                cell.appendChild(pieceElement);
            }

            // Add event listeners for drag-and-drop
            cell.addEventListener('dragover', dragOver);
            cell.addEventListener('drop', drop);

            boardElement.appendChild(cell);
        }
    }
}

// Get the symbol for a piece type
function getPieceSymbol(type) {
    const symbols = {
        rook: '車',
        horse: '馬',
        elephant: '象',
        advisor: '士',
        general: '將',
        cannon: '炮',
        soldier: '兵',
    };
    return symbols[type] || '';
}

// Handle drag start
function dragStart(e) {
    const pieceElement = e.target;
    const row = pieceElement.dataset.row;
    const col = pieceElement.dataset.col;

    e.dataTransfer.setData('text/plain', JSON.stringify({ row, col }));
    pieceElement.style.opacity = '0.4';
}

// Handle drag over
function dragOver(e) {
    e.preventDefault();
}

// Handle drop
function drop(e) {
    e.preventDefault();
    const cell = e.target;
    const data = JSON.parse(e.dataTransfer.getData('text/plain'));
    const sourceRow = parseInt(data.row);
    const sourceCol = parseInt(data.col);
    const targetRow = parseInt(cell.dataset.row);
    const targetCol = parseInt(cell.dataset.col);

    // Validate the move (placeholder for now)
    const isValidMove = true; // Replace with actual validation logic

    if (isValidMove) {
        // Move the piece
        const piece = gameState.board[sourceRow][sourceCol];
        gameState.board[sourceRow][sourceCol] = null;
        gameState.board[targetRow][targetCol] = piece;

        // Update the piece's position
        piece.row = targetRow;
        piece.col = targetCol;

        // Log the move
        gameState.moveHistory.push({
            from: { row: sourceRow, col: sourceCol },
            to: { row: targetRow, col: targetCol },
            piece: piece.type,
            color: piece.color,
        });

        // Switch turns
        gameState.currentPlayer = gameState.currentPlayer === 'red' ? 'black' : 'red';

        // Re-render the board
        renderBoard();

        // Update the turn indicator
        updateTurnIndicator();

        // Update the move history in the sidebar
        updateMoveHistory();
    }
}

// Update the turn indicator in the sidebar
function updateTurnIndicator() {
    const turnIndicator = document.querySelector('.turn-indicator');
    turnIndicator.textContent = `Current Turn: ${gameState.currentPlayer === 'red' ? 'Red' : 'Black'}`;
}

// Update the move history in the sidebar
function updateMoveHistory() {
    const moveHistoryElement = document.querySelector('.move-history ul');
    moveHistoryElement.innerHTML = '';

    gameState.moveHistory.forEach(move => {
        const li = document.createElement('li');
        li.textContent = `${move.color} ${move.piece}: (${move.from.row},${move.from.col}) → (${move.to.row},${move.to.col})`;
        moveHistoryElement.appendChild(li);
    });
}

// Initialize the game when the page loads
window.addEventListener('DOMContentLoaded', initializeGame);