// Game Logic Implementation for Gomoku (五子棋)

const board = document.getElementById('board');
let currentPlayer = 'black';
const boardSize = 15;
const gameBoard = Array(boardSize).fill().map(() => Array(boardSize).fill(null));

// Initialize the board
function initializeBoard() {
    for (let i = 0; i < boardSize; i++) {
        for (let j = 0; j < boardSize; j++) {
            const cell = document.createElement('div');
            cell.classList.add('cell');
            cell.dataset.row = i;
            cell.dataset.col = j;
            cell.addEventListener('click', handleCellClick);
            board.appendChild(cell);
        }
    }
}

// Handle cell click event
function handleCellClick(event) {
    const cell = event.target;
    const row = parseInt(cell.dataset.row);
    const col = parseInt(cell.dataset.col);

    // Skip if the cell is already occupied
    if (gameBoard[row][col] !== null) return;

    // Place the piece
    const piece = document.createElement('div');
    piece.classList.add(currentPlayer);
    cell.appendChild(piece);
    gameBoard[row][col] = currentPlayer;

    // Check for a win
    if (checkWin(row, col)) {
        setTimeout(() => alert(`${currentPlayer.toUpperCase()} wins!`), 10);
        resetGame();
        return;
    }

    // Switch player
    currentPlayer = currentPlayer === 'black' ? 'white' : 'black';
}

// Check for a win condition (5 in a row)
function checkWin(row, col) {
    const directions = [
        [1, 0],   // Horizontal
        [0, 1],   // Vertical
        [1, 1],   // Diagonal down-right
        [1, -1]   // Diagonal down-left
    ];

    for (const [dx, dy] of directions) {
        let count = 1;

        // Check in the positive direction
        count += countDirection(row, col, dx, dy);
        // Check in the negative direction
        count += countDirection(row, col, -dx, -dy);

        if (count >= 5) return true;
    }

    return false;
}

// Count consecutive pieces in a direction
function countDirection(row, col, dx, dy) {
    let count = 0;
    let r = row + dx;
    let c = col + dy;

    while (r >= 0 && r < boardSize && c >= 0 && c < boardSize && gameBoard[r][c] === currentPlayer) {
        count++;
        r += dx;
        c += dy;
    }

    return count;
}

// Reset the game
function resetGame() {
    gameBoard.forEach(row => row.fill(null));
    document.querySelectorAll('.cell').forEach(cell => {
        while (cell.firstChild) {
            cell.removeChild(cell.firstChild);
        }
    });
    currentPlayer = 'black';
}

// Initialize the game
initializeBoard();