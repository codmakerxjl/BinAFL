// pieceMovementRules.js

/**
 * Defines the movement rules for each Chinese Chess piece.
 * Each function checks if a move from (fromRow, fromCol) to (toRow, toCol) is valid.
 * Assumes the board is a 10x9 grid (rows x columns).
 */

/**
 * Validates a move for the General (King).
 * The General moves one step orthogonally within the palace.
 */
function isValidGeneralMove(fromRow, fromCol, toRow, toCol, board) {
    // Check if the move is within the palace
    const isRed = board[fromRow][fromCol].color === 'red';
    const palaceRows = isRed ? [7, 8, 9] : [0, 1, 2];
    const palaceCols = [3, 4, 5];

    if (!palaceRows.includes(toRow) || !palaceCols.includes(toCol)) {
        return false;
    }

    // Check if the move is one step orthogonally
    const rowDiff = Math.abs(toRow - fromRow);
    const colDiff = Math.abs(toCol - fromCol);

    return (rowDiff === 1 && colDiff === 0) || (rowDiff === 0 && colDiff === 1);
}

/**
 * Validates a move for the Advisor.
 * The Advisor moves one step diagonally within the palace.
 */
function isValidAdvisorMove(fromRow, fromCol, toRow, toCol, board) {
    // Check if the move is within the palace
    const isRed = board[fromRow][fromCol].color === 'red';
    const palaceRows = isRed ? [7, 8, 9] : [0, 1, 2];
    const palaceCols = [3, 4, 5];

    if (!palaceRows.includes(toRow) || !palaceCols.includes(toCol)) {
        return false;
    }

    // Check if the move is one step diagonally
    const rowDiff = Math.abs(toRow - fromRow);
    const colDiff = Math.abs(toCol - fromCol);

    return rowDiff === 1 && colDiff === 1;
}

/**
 * Validates a move for the Elephant.
 * The Elephant moves two steps diagonally and cannot cross the river.
 */
function isValidElephantMove(fromRow, fromCol, toRow, toCol, board) {
    const isRed = board[fromRow][fromCol].color === 'red';
    const riverRow = isRed ? 4 : 5;

    // Check if the Elephant is crossing the river
    if ((isRed && toRow <= riverRow) || (!isRed && toRow >= riverRow)) {
        return false;
    }

    // Check if the move is two steps diagonally
    const rowDiff = Math.abs(toRow - fromRow);
    const colDiff = Math.abs(toCol - fromCol);

    if (rowDiff !== 2 || colDiff !== 2) {
        return false;
    }

    // Check if the intervening square is empty
    const midRow = (fromRow + toRow) / 2;
    const midCol = (fromCol + toCol) / 2;

    return board[midRow][midCol] === null;
}

/**
 * Validates a move for the Horse.
 * The Horse moves one step orthogonally and then one step diagonally (cannot jump over obstacles).
 */
function isValidHorseMove(fromRow, fromCol, toRow, toCol, board) {
    const rowDiff = Math.abs(toRow - fromRow);
    const colDiff = Math.abs(toCol - fromCol);

    // Check the "L" shape move
    if (!((rowDiff === 2 && colDiff === 1) || (rowDiff === 1 && colDiff === 2))) {
        return false;
    }

    // Check if the intervening square is empty
    if (rowDiff === 2) {
        const midRow = (fromRow + toRow) / 2;
        return board[midRow][fromCol] === null;
    } else {
        const midCol = (fromCol + toCol) / 2;
        return board[fromRow][midCol] === null;
    }
}

/**
 * Validates a move for the Chariot (Rook).
 * The Chariot moves any number of steps orthogonally (path must be clear).
 */
function isValidChariotMove(fromRow, fromCol, toRow, toCol, board) {
    // Check if the move is orthogonal
    if (fromRow !== toRow && fromCol !== toCol) {
        return false;
    }

    // Check if the path is clear
    if (fromRow === toRow) {
        const start = Math.min(fromCol, toCol);
        const end = Math.max(fromCol, toCol);
        for (let col = start + 1; col < end; col++) {
            if (board[fromRow][col] !== null) {
                return false;
            }
        }
    } else {
        const start = Math.min(fromRow, toRow);
        const end = Math.max(fromRow, toRow);
        for (let row = start + 1; row < end; row++) {
            if (board[row][fromCol] !== null) {
                return false;
            }
        }
    }

    return true;
}

/**
 * Validates a move for the Cannon.
 * The Cannon moves like a Chariot but must jump over exactly one piece to capture.
 */
function isValidCannonMove(fromRow, fromCol, toRow, toCol, board) {
    // Check if the move is orthogonal
    if (fromRow !== toRow && fromCol !== toCol) {
        return false;
    }

    const targetPiece = board[toRow][toCol];
    let pieceCount = 0;

    // Count pieces along the path
    if (fromRow === toRow) {
        const start = Math.min(fromCol, toCol);
        const end = Math.max(fromCol, toCol);
        for (let col = start + 1; col < end; col++) {
            if (board[fromRow][col] !== null) {
                pieceCount++;
            }
        }
    } else {
        const start = Math.min(fromRow, toRow);
        const end = Math.max(fromRow, toRow);
        for (let row = start + 1; row < end; row++) {
            if (board[row][fromCol] !== null) {
                pieceCount++;
            }
        }
    }

    // If capturing, there must be exactly one piece in between
    if (targetPiece !== null) {
        return pieceCount === 1;
    }
    // If not capturing, the path must be clear
    return pieceCount === 0;
}

/**
 * Validates a move for the Soldier (Pawn).
 * The Soldier moves one step forward or sideways after crossing the river.
 */
function isValidSoldierMove(fromRow, fromCol, toRow, toCol, board) {
    const isRed = board[fromRow][fromCol].color === 'red';
    const rowDiff = toRow - fromRow;
    const colDiff = Math.abs(toCol - fromCol);

    // Check if the Soldier has crossed the river
    const hasCrossedRiver = isRed ? fromRow <= 4 : fromRow >= 5;

    if (hasCrossedRiver) {
        // Can move one step forward or sideways
        if (isRed) {
            return (rowDiff === -1 && colDiff === 0) || (rowDiff === 0 && colDiff === 1);
        } else {
            return (rowDiff === 1 && colDiff === 0) || (rowDiff === 0 && colDiff === 1);
        }
    } else {
        // Can only move one step forward
        if (isRed) {
            return rowDiff === -1 && colDiff === 0;
        } else {
            return rowDiff === 1 && colDiff === 0;
        }
    }
}

// Export the functions
module.exports = {
    isValidGeneralMove,
    isValidAdvisorMove,
    isValidElephantMove,
    isValidHorseMove,
    isValidChariotMove,
    isValidCannonMove,
    isValidSoldierMove
};