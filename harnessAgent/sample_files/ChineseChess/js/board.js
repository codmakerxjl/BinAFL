// Board Representation for Chinese Chess

/**
 * Represents the Chinese Chess board as a 9x10 grid.
 * Each cell can contain a piece object or be null (empty).
 */
class Board {
    constructor() {
        this.grid = Array(10).fill().map(() => Array(9).fill(null));
        this.initializePieces();
    }

    /**
     * Initializes the board with pieces in their standard starting positions.
     */
    initializePieces() {
        // Red pieces (top side)
        this.grid[0][0] = { type: 'CHARIOT', color: 'red', symbol: '車' };
        this.grid[0][1] = { type: 'HORSE', color: 'red', symbol: '馬' };
        this.grid[0][2] = { type: 'ELEPHANT', color: 'red', symbol: '相' };
        this.grid[0][3] = { type: 'ADVISOR', color: 'red', symbol: '仕' };
        this.grid[0][4] = { type: 'GENERAL', color: 'red', symbol: '帥' };
        this.grid[0][5] = { type: 'ADVISOR', color: 'red', symbol: '仕' };
        this.grid[0][6] = { type: 'ELEPHANT', color: 'red', symbol: '相' };
        this.grid[0][7] = { type: 'HORSE', color: 'red', symbol: '馬' };
        this.grid[0][8] = { type: 'CHARIOT', color: 'red', symbol: '車' };
        this.grid[2][1] = { type: 'CANNON', color: 'red', symbol: '炮' };
        this.grid[2][7] = { type: 'CANNON', color: 'red', symbol: '炮' };
        this.grid[3][0] = { type: 'SOLDIER', color: 'red', symbol: '兵' };
        this.grid[3][2] = { type: 'SOLDIER', color: 'red', symbol: '兵' };
        this.grid[3][4] = { type: 'SOLDIER', color: 'red', symbol: '兵' };
        this.grid[3][6] = { type: 'SOLDIER', color: 'red', symbol: '兵' };
        this.grid[3][8] = { type: 'SOLDIER', color: 'red', symbol: '兵' };

        // Black pieces (bottom side)
        this.grid[9][0] = { type: 'CHARIOT', color: 'black', symbol: '車' };
        this.grid[9][1] = { type: 'HORSE', color: 'black', symbol: '馬' };
        this.grid[9][2] = { type: 'ELEPHANT', color: 'black', symbol: '象' };
        this.grid[9][3] = { type: 'ADVISOR', color: 'black', symbol: '士' };
        this.grid[9][4] = { type: 'GENERAL', color: 'black', symbol: '將' };
        this.grid[9][5] = { type: 'ADVISOR', color: 'black', symbol: '士' };
        this.grid[9][6] = { type: 'ELEPHANT', color: 'black', symbol: '象' };
        this.grid[9][7] = { type: 'HORSE', color: 'black', symbol: '馬' };
        this.grid[9][8] = { type: 'CHARIOT', color: 'black', symbol: '車' };
        this.grid[7][1] = { type: 'CANNON', color: 'black', symbol: '炮' };
        this.grid[7][7] = { type: 'CANNON', color: 'black', symbol: '炮' };
        this.grid[6][0] = { type: 'SOLDIER', color: 'black', symbol: '卒' };
        this.grid[6][2] = { type: 'SOLDIER', color: 'black', symbol: '卒' };
        this.grid[6][4] = { type: 'SOLDIER', color: 'black', symbol: '卒' };
        this.grid[6][6] = { type: 'SOLDIER', color: 'black', symbol: '卒' };
        this.grid[6][8] = { type: 'SOLDIER', color: 'black', symbol: '卒' };
    }

    /**
     * Returns the piece at the specified position.
     * @param {number} row - The row index (0-9).
     * @param {number} col - The column index (0-8).
     * @returns {Object|null} The piece object or null if the cell is empty.
     */
    getPiece(row, col) {
        return this.grid[row][col];
    }

    /**
     * Moves a piece from one position to another.
     * @param {number} fromRow - The source row index.
     * @param {number} fromCol - The source column index.
     * @param {number} toRow - The target row index.
     * @param {number} toCol - The target column index.
     * @returns {boolean} True if the move was successful, false otherwise.
     */
    movePiece(fromRow, fromCol, toRow, toCol) {
        const piece = this.grid[fromRow][fromCol];
        if (!piece) return false;

        this.grid[toRow][toCol] = piece;
        this.grid[fromRow][fromCol] = null;
        return true;
    }

    /**
     * Checks if a cell is empty.
     * @param {number} row - The row index.
     * @param {number} col - The column index.
     * @returns {boolean} True if the cell is empty, false otherwise.
     */
    isEmpty(row, col) {
        return this.grid[row][col] === null;
    }

    /**
     * Resets the board to its initial state.
     */
    reset() {
        this.grid = Array(10).fill().map(() => Array(9).fill(null));
        this.initializePieces();
    }
}

export default Board;