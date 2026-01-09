#!/usr/bin/env python3
"""
Example: Custom AI implementation for ZZmazon
This example shows how to create a more advanced AI.
"""

import sys
sys.path.insert(0, '..')

from zzmazon import AmazonsGame
import random


class AdvancedAI:
    """
    A more advanced AI that uses simple heuristics.
    
    Heuristics:
    1. Maximize mobility (number of available moves)
    2. Control center squares
    3. Stay spread out
    """
    
    def __init__(self, game: AmazonsGame):
        self.game = game
    
    def evaluate_move(self, move):
        """
        Evaluate a move based on simple heuristics.
        Returns a score (higher is better).
        """
        from_row, from_col, to_row, to_col, arrow_row, arrow_col = move
        score = 0
        
        # Make temporary move
        piece = self.game.board[from_row][from_col]
        self.game.board[from_row][from_col] = ' '
        self.game.board[to_row][to_col] = piece
        self.game.board[arrow_row][arrow_col] = 'X'
        
        # Heuristic 1: Maximize mobility after move
        mobility = len(self.game.get_queen_moves(to_row, to_col))
        score += mobility * 2
        
        # Heuristic 2: Control center (prefer positions near center)
        center = self.game.board_size / 2
        distance_from_center = abs(to_row - center) + abs(to_col - center)
        score -= distance_from_center * 0.5
        
        # Heuristic 3: Block opponent's mobility
        # (simplified - just place arrows in central areas)
        arrow_center_distance = abs(arrow_row - center) + abs(arrow_col - center)
        score -= arrow_center_distance * 0.3
        
        # Restore board
        self.game.board[arrow_row][arrow_col] = ' '
        self.game.board[to_row][to_col] = ' '
        self.game.board[from_row][from_col] = piece
        
        return score
    
    def get_move(self):
        """Get the best move according to heuristics."""
        moves = self.game.get_all_valid_moves()
        if not moves:
            return None
        
        # Evaluate all moves and pick the best
        best_move = None
        best_score = float('-inf')
        
        # Sample moves if there are too many (for performance)
        if len(moves) > 100:
            moves = random.sample(moves, 100)
        
        for move in moves:
            score = self.evaluate_move(move)
            if score > best_score:
                best_score = score
                best_move = move
        
        return best_move


def play_advanced_ai_demo():
    """Play a demo game with the advanced AI."""
    print("=" * 60)
    print("ZZmazon - Advanced AI Demo")
    print("=" * 60)
    
    game = AmazonsGame()
    ai = AdvancedAI(game)
    
    move_num = 0
    while not game.is_game_over() and move_num < 50:
        game.display_board()
        print(f"Current player: {'White' if game.current_player == 'W' else 'Black'}")
        print(f"Move #{game.move_count + 1}")
        
        move = ai.get_move()
        if move:
            from_row, from_col, to_row, to_col, arrow_row, arrow_col = move
            print(f"AI plays: {from_row} {from_col} -> {to_row} {to_col}, arrow: {arrow_row} {arrow_col}")
            game.make_move(from_row, from_col, to_row, to_col, arrow_row, arrow_col)
            
            # Pause for readability
            input("Press Enter for next move...")
        else:
            break
        
        move_num += 1
    
    game.display_board()
    winner = game.get_winner()
    if winner:
        print(f"\nGame Over! Winner: {'White' if winner == 'W' else 'Black'}")
    else:
        print("\nGame ended (move limit reached)")


if __name__ == "__main__":
    play_advanced_ai_demo()
