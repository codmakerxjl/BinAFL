document.addEventListener('DOMContentLoaded', () => {
    const cells = document.querySelectorAll('.cell');
    let currentPlayer = 'black';

    cells.forEach(cell => {
        cell.addEventListener('click', () => {
            if (!cell.textContent) {
                const piece = document.createElement('div');
                piece.style.width = '30px';
                piece.style.height = '30px';
                piece.style.borderRadius = '50%';
                piece.style.backgroundColor = currentPlayer === 'black' ? '#000' : '#fff';
                piece.style.border = currentPlayer === 'black' ? '1px solid #333' : '1px solid #999';
                cell.appendChild(piece);
                currentPlayer = currentPlayer === 'black' ? 'white' : 'black';
            }
        });
    });
});