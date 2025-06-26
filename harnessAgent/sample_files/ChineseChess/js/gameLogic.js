// Chinese Chess Piece Move Validation

// Constants for piece types
const PIECE_TYPES = {
  GENERAL: 'general',
  ADVISOR: 'advisor',
  ELEPHANT: 'elephant',
  HORSE: 'horse',
  CHARIOT: 'chariot',
  CANNON: 'cannon',
  SOLDIER: 'soldier'
};

// Validate move for a given piece
function isValidMove(piece, fromRow, fromCol, toRow, toCol, board) {
  switch (piece.type) {
    case PIECE_TYPES.GENERAL:
      return validateGeneral(piece, fromRow, fromCol, toRow, toCol, board);
    case PIECE_TYPES.ADVISOR:
      return validateAdvisor(piece, fromRow, fromCol, toRow, toCol, board);
    case PIECE_TYPES.ELEPHANT:
      return validateElephant(piece, fromRow, fromCol, toRow, toCol, board);
    case PIECE_TYPES.HORSE:
      return validateHorse(piece, fromRow, fromCol, toRow, toCol, board);
    case PIECE_TYPES.CHARIOT:
      return validateChariot(piece, fromRow, fromCol, toRow, toCol, board);
    case PIECE_TYPES.CANNON:
      return validateCannon(piece, fromRow, fromCol, toRow, toCol, board);
    case PIECE_TYPES.SOLDIER:
      return validateSoldier(piece, fromRow, fromCol, toRow, toCol, board);
    default:
      return false;
  }
}

// General (King) move validation
function validateGeneral(piece, fromRow, fromCol, toRow, toCol, board) {
  // General can move one step orthogonally within the palace
  const isWithinPalace = (row, col) => {
    return (row >= 0 && row <= 2 && col >= 3 && col <= 5) ||
           (row >= 7 && row <= 9 && col >= 3 && col <= 5);
  };

  const rowDiff = Math.abs(toRow - fromRow);
  const colDiff = Math.abs(toCol - fromCol);

  return isWithinPalace(toRow, toCol) &&
         ((rowDiff === 1 && colDiff === 0) || (rowDiff === 0 && colDiff === 1));
}

// Advisor move validation
function validateAdvisor(piece, fromRow, fromCol, toRow, toCol, board) {
  // Advisor moves one step diagonally within the palace
  const isWithinPalace = (row, col) => {
    return (row >= 0 && row <= 2 && col >= 3 && col <= 5) ||
           (row >= 7 && row <= 9 && col >= 3 && col <= 5);
  };

  const rowDiff = Math.abs(toRow - fromRow);
  const colDiff = Math.abs(toCol - fromCol);

  return isWithinPalace(toRow, toCol) &&
         rowDiff === 1 && colDiff === 1;
}

// Elephant move validation
function validateElephant(piece, fromRow, fromCol, toRow, toCol, board) {
  // Elephant moves two steps diagonally and cannot cross the river
  const rowDiff = Math.abs(toRow - fromRow);
  const colDiff = Math.abs(toCol - fromCol);

  if (rowDiff !== 2 || colDiff !== 2) return false;

  // Check if the elephant crosses the river
  if ((piece.color === 'red' && toRow > 4) ||
      (piece.color === 'black' && toRow < 5)) {
    return false;
  }

  // Check if the intervening square is blocked
  const midRow = (fromRow + toRow) / 2;
  const midCol = (fromCol + toCol) / 2;
  return !board[midRow][midCol];
}

// Horse move validation
function validateHorse(piece, fromRow, fromCol, toRow, toCol, board) {
  // Horse moves one step orthogonally and then one step diagonally
  const rowDiff = Math.abs(toRow - fromRow);
  const colDiff = Math.abs(toCol - fromCol);

  if (!((rowDiff === 2 && colDiff === 1) || (rowDiff === 1 && colDiff === 2))) {
    return false;
  }

  // Check if the horse is blocked
  if (rowDiff === 2) {
    const midRow = (fromRow + toRow) / 2;
    return !board[midRow][fromCol];
  } else {
    const midCol = (fromCol + toCol) / 2;
    return !board[fromRow][midCol];
  }
}

// Chariot (Rook) move validation
function validateChariot(piece, fromRow, fromCol, toRow, toCol, board) {
  // Chariot moves any number of steps orthogonally
  if (fromRow !== toRow && fromCol !== toCol) return false;

  // Check if the path is clear
  if (fromRow === toRow) {
    const start = Math.min(fromCol, toCol);
    const end = Math.max(fromCol, toCol);
    for (let col = start + 1; col < end; col++) {
      if (board[fromRow][col]) return false;
    }
  } else {
    const start = Math.min(fromRow, toRow);
    const end = Math.max(fromRow, toRow);
    for (let row = start + 1; row < end; row++) {
      if (board[row][fromCol]) return false;
    }
  }

  return true;
}

// Cannon move validation
function validateCannon(piece, fromRow, fromCol, toRow, toCol, board) {
  // Cannon moves like a chariot but must jump over exactly one piece to capture
  if (fromRow !== toRow && fromCol !== toCol) return false;

  let pieceCount = 0;
  if (fromRow === toRow) {
    const start = Math.min(fromCol, toCol);
    const end = Math.max(fromCol, toCol);
    for (let col = start + 1; col < end; col++) {
      if (board[fromRow][col]) pieceCount++;
    }
  } else {
    const start = Math.min(fromRow, toRow);
    const end = Math.max(fromRow, toRow);
    for (let row = start + 1; row < end; row++) {
      if (board[row][fromCol]) pieceCount++;
    }
  }

  const targetPiece = board[toRow][toCol];
  if (targetPiece) {
    // Capture: must jump over exactly one piece
    return pieceCount === 1 && targetPiece.color !== piece.color;
  } else {
    // Move: no pieces in the way
    return pieceCount === 0;
  }
}

// Soldier (Pawn) move validation
function validateSoldier(piece, fromRow, fromCol, toRow, toCol, board) {
  // Soldier moves one step forward or sideways after crossing the river
  const rowDiff = toRow - fromRow;
  const colDiff = Math.abs(toCol - fromCol);

  if (piece.color === 'red') {
    if (fromRow > 4) {
      // Crossed the river: can move sideways
      return (rowDiff === 0 && colDiff === 1) || rowDiff === -1;
    } else {
      // Before crossing the river: only forward
      return rowDiff === -1 && colDiff === 0;
    }
  } else {
    if (fromRow < 5) {
      // Crossed the river: can move sideways
      return (rowDiff === 0 && colDiff === 1) || rowDiff === 1;
    } else {
      // Before crossing the river: only forward
      return rowDiff === 1 && colDiff === 0;
    }
  }
}

// Export the validation function
module.exports = { isValidMove, PIECE_TYPES };