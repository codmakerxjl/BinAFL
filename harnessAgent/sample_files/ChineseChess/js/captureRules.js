// captureRules.js

/**
 * Checks if a move results in a capture and handles the removal of the captured piece.
 * @param {Object} board - The current board state.
 * @param {string} currentPlayer - The current player ('red' or 'black').
 * @param {number} fromRow - The source row of the move.
 * @param {number} fromCol - The source column of the move.
 * @param {number} toRow - The target row of the move.
 * @param {number} toCol - The target column of the move.
 * @returns {boolean} - True if the move results in a capture, false otherwise.
 */
function checkAndHandleCapture(board, currentPlayer, fromRow, fromCol, toRow, toCol) {
    const targetPiece = board[toRow][toCol];

    // Check if the target cell contains an opponent's piece
    if (targetPiece && targetPiece.color !== currentPlayer) {
        // Remove the captured piece from the board
        board[toRow][toCol] = null;
        return true;
    }

    return false;
}

/**
 * Determines if a capture is valid based on the piece type and game rules.
 * @param {Object} piece - The piece attempting to capture.
 * @param {Object} targetPiece - The piece being captured.
 * @param {Object} board - The current board state.
 * @param {number} fromRow - The source row of the move.
 * @param {number} fromCol - The source column of the move.
 * @param {number} toRow - The target row of the move.
 * @param {number} toCol - The target column of the move.
 * @returns {boolean} - True if the capture is valid, false otherwise.
 */
function isValidCapture(piece, targetPiece, board, fromRow, fromCol, toRow, toCol) {
    // General rule: A piece can capture an opponent's piece if the move is valid
    // Special cases (e.g., Cannon) are handled separately
    return targetPiece && targetPiece.color !== piece.color;
}

// Export the functions for use in gameLogic.js
export { checkAndHandleCapture, isValidCapture };