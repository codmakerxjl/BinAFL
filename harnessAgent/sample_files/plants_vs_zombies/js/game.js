// Initialize game state
const gameState = {
    sunCount: 50,
    plants: [],
    zombies: [],
};

// DOM elements
const sunCountElement = document.getElementById('sun-count');
const lawnElement = document.getElementById('lawn');

// Update sun count display
function updateSunCount() {
    sunCountElement.textContent = gameState.sunCount;
}

// Add sun to the counter
function addSun(amount) {
    gameState.sunCount += amount;
    updateSunCount();
}

// Place a plant on the lawn
function placePlant(plantType, cellIndex) {
    if (gameState.sunCount >= plantType.cost) {
        gameState.sunCount -= plantType.cost;
        updateSunCount();
        gameState.plants.push({
            type: plantType,
            cellIndex,
            health: plantType.health,
        });
        // TODO: Render plant on the lawn
    }
}

// Initialize the game
function initGame() {
    updateSunCount();
    // TODO: Initialize lawn grid and plant selection UI
}

// Start the game
window.onload = initGame;