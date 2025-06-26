document.getElementById('earForm').addEventListener('submit', function(event) {
    event.preventDefault();

    const earShape = document.getElementById('earShape').value;
    const earSize = document.getElementById('earSize').value;

    const resultDiv = document.getElementById('result');
    resultDiv.textContent = `Identified Traits: Shape - ${earShape}, Size - ${earSize}`;
});