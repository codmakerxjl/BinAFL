// Initialize the game board and pieces
const board = document.querySelector('.board');
const cells = [];

// Define the initial positions of the pieces
const pieces = [
    { type: 'rook', color: 'black', row: 0, col: 0 },
    { type: 'horse', color: 'black', row: 0, col: 1 },
    { type: 'elephant', color: 'black', row: 0, col: 2 },
    { type: 'advisor', color: 'black', row: 0, col: 3 },
    { type: 'general', color: 'black', row: 0, col: 4 },
    { type: 'advisor', color: 'black', row: 0, col: 5 },
    { type: 'elephant', color: 'black', row: 0, col: 6 },
    { type: 'horse', color: 'black', row: 0, col: 7 },
    { type: 'rook', color: 'black', row: 0, col: 8 },
    { type: 'cannon', color: 'black', row: 2, col: 1 },
    { type: 'cannon', color: 'black', row: 2, col: 7 },
    { type: 'soldier', color: 'black', row: 3, col: 0 },
    { type: 'soldier', color: 'black', row: 3, col: 2 },
    { type: 'soldier', color: 'black', row: 3, col: 4 },
    { type: 'soldier', color: 'black', row: 3, col: 6 },
    { type: 'soldier', color: 'black', row: 3, col: 8 },
    { type: 'rook', color: 'red', row: 9, col: 0 },
    { type: 'horse', color: 'red', row: 9, col: 1 },
    { type: 'elephant', color: 'red', row: 9, col: 2 },
    { type: 'advisor', color: 'red', row: 9, col: 3 },
    { type: 'general', color: 'red', row: 9, col: 4 },
    { type: 'advisor', color: 'red', row: 9, col: 5 },
    { type: 'elephant', color: 'red', row: 9, col: 6 },
    { type: 'horse', color: 'red', row: 9, col: 7 },
    { type: 'rook', color: 'red', row: 9, col: 8 },
    { type: 'cannon', color: 'red', row: 7, col: 1 },
    { type: 'cannon', color: 'red', row: 7, col: 7 },
    { type: 'soldier', color: 'red', row: 6, col: 0 },
    { type: 'soldier', color: 'red', row: 6, col: 2 },
    { type: 'soldier', color: 'red', row: 6, col: 4 },
    { type: 'soldier', color: 'red', row: 6, col: 6 },
    { type: 'soldier', color: 'red', row: 6, col: 8 }
];

// Create the board cells and place the pieces
function initializeBoard() {
    for (let row = 0; row < 10; row++) {
        cells[row] = [];
        for (let col = 0; col < 9; col++) {
            const cell = document.createElement('div');
            cell.className = 'cell';
            cell.dataset.row = row;
            cell.dataset.col = col;
            board.appendChild(cell);
            cells[row][col] = cell;
        }
    }

    // Place the pieces on the board
    pieces.forEach(piece => {
        const cell = cells[piece.row][piece.col];
        const pieceElement = document.createElement('div');
        pieceElement.className = `piece ${piece.color}-piece`;
        pieceElement.textContent = getPieceSymbol(piece.type);
        pieceElement.dataset.type = piece.type;
        pieceElement.dataset.color = piece.color;
        cell.appendChild(pieceElement);
    });
}

// Helper function to get the symbol for each piece type
function getPieceSymbol(type) {
    const symbols = {
        'rook': '車',
        'horse': '馬',
        'elephant': '相',
        'advisor': '仕',
        'general': '帥',
        'cannon': '炮',
        'soldier': '兵'
    };
    return symbols[type] || '';
}

// Initialize the game when the DOM is fully loaded
document.addEventListener('DOMContentLoaded', initializeBoard);