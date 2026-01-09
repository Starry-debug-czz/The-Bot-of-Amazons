# Examples for ZZmazon

This directory contains example implementations and extensions for ZZmazon.

## Files

### `botzone_adapter.py`
Demonstrates how to adapt ZZmazon for the Botzone platform. This adapter:
- Reads game state from Botzone's JSON format
- Generates moves using the ZZmazon engine
- Outputs moves in Botzone's expected format

**Usage:**
```bash
python botzone_adapter.py
```

### `advanced_ai.py`
Shows how to implement a more sophisticated AI using heuristic evaluation. This AI considers:
- Mobility (number of available moves after the move)
- Control of center squares
- Strategic arrow placement to block opponent

**Usage:**
```bash
python advanced_ai.py
```

## Creating Your Own AI

To create your own AI implementation:

1. Import the game engine:
```python
from zzmazon import AmazonsGame
```

2. Create your AI class:
```python
class MyAI:
    def __init__(self, game: AmazonsGame):
        self.game = game
    
    def get_move(self):
        moves = self.game.get_all_valid_moves()
        # Your logic here to select the best move
        return best_move  # Returns (from_row, from_col, to_row, to_col, arrow_row, arrow_col)
```

3. Use it in a game:
```python
game = AmazonsGame()
ai = MyAI(game)

while not game.is_game_over():
    move = ai.get_move()
    if move:
        game.make_move(*move)
```

## Advanced Techniques

For competitive AI implementations, consider:

- **Minimax with Alpha-Beta Pruning**: Classic game tree search
- **Monte Carlo Tree Search (MCTS)**: Effective for games with large branching factors
- **Neural Networks**: Deep learning for position evaluation
- **Opening Books**: Pre-computed optimal opening moves
- **Endgame Databases**: Pre-solved endgame positions

## Botzone Integration

To submit to Botzone:
1. Modify `botzone_adapter.py` with your AI implementation
2. Test locally with sample inputs
3. Submit to Botzone platform following their guidelines
4. Ensure your code runs within time and memory limits

## Contributing

Feel free to contribute your own examples! See the main repository README for contribution guidelines.
