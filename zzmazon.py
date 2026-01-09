#!/usr/bin/env python3
"""
ZZmazon - A Game of the Amazons Implementation
Copyright (c) 2026 Zhizhong Chen
Licensed under MIT License
"""

import sys
from typing import List, Tuple, Optional, Set
import copy


class AmazonsGame:
    """
    Implements the Game of the Amazons on a 10x10 board.
    
    Rules:
    - Two players (White and Black) each control 4 amazons
    - On each turn, a player:
      1. Moves one of their amazons (like a chess queen)
      2. Shoots an arrow from the new position (like a chess queen)
    - The arrow creates a burned square that blocks future moves
    - The player who cannot move loses
    """
    
    def __init__(self, board_size: int = 10):
        self.board_size = board_size
        self.board = [[' ' for _ in range(board_size)] for _ in range(board_size)]
        self.current_player = 'W'  # White starts
        self.move_count = 0
        self._initialize_board()
    
    def _initialize_board(self):
        """Initialize the board with standard starting positions."""
        # White amazons (W)
        initial_white = [(0, 3), (0, 6), (3, 0), (3, 9)]
        # Black amazons (B)
        initial_black = [(6, 0), (6, 9), (9, 3), (9, 6)]
        
        for row, col in initial_white:
            self.board[row][col] = 'W'
        
        for row, col in initial_black:
            self.board[row][col] = 'B'
    
    def display_board(self):
        """Display the current board state."""
        print("\n   ", end="")
        for i in range(self.board_size):
            print(f" {i} ", end="")
        print()
        
        for i in range(self.board_size):
            print(f" {i} ", end="")
            for j in range(self.board_size):
                cell = self.board[i][j]
                if cell == 'W':
                    symbol = ' W '
                elif cell == 'B':
                    symbol = ' B '
                elif cell == 'X':
                    symbol = ' X '
                else:
                    symbol = ' . '
                print(symbol, end="")
            print()
        print()
    
    def is_valid_position(self, row: int, col: int) -> bool:
        """Check if a position is within board bounds."""
        return 0 <= row < self.board_size and 0 <= col < self.board_size
    
    def is_empty(self, row: int, col: int) -> bool:
        """Check if a position is empty."""
        return self.is_valid_position(row, col) and self.board[row][col] == ' '
    
    def get_queen_moves(self, from_row: int, from_col: int) -> List[Tuple[int, int]]:
        """Get all valid queen-like moves from a position."""
        moves = []
        directions = [(-1, -1), (-1, 0), (-1, 1), (0, -1), 
                     (0, 1), (1, -1), (1, 0), (1, 1)]
        
        for dr, dc in directions:
            row, col = from_row + dr, from_col + dc
            while self.is_valid_position(row, col):
                if self.board[row][col] != ' ':
                    break
                moves.append((row, col))
                row += dr
                col += dc
        
        return moves
    
    def is_valid_move(self, from_row: int, from_col: int, 
                     to_row: int, to_col: int,
                     arrow_row: int, arrow_col: int) -> bool:
        """Check if a complete move (amazon move + arrow) is valid."""
        # Check if from position has current player's amazon
        if not self.is_valid_position(from_row, from_col):
            return False
        if self.board[from_row][from_col] != self.current_player:
            return False
        
        # Check if amazon move is valid
        valid_amazon_moves = self.get_queen_moves(from_row, from_col)
        if (to_row, to_col) not in valid_amazon_moves:
            return False
        
        # Temporarily move the amazon to check arrow validity
        piece = self.board[from_row][from_col]
        self.board[from_row][from_col] = ' '
        self.board[to_row][to_col] = piece
        
        # Check if arrow shot is valid
        valid_arrow_shots = self.get_queen_moves(to_row, to_col)
        is_valid = (arrow_row, arrow_col) in valid_arrow_shots
        
        # Restore board state
        self.board[to_row][to_col] = ' '
        self.board[from_row][from_col] = piece
        
        return is_valid
    
    def make_move(self, from_row: int, from_col: int,
                 to_row: int, to_col: int,
                 arrow_row: int, arrow_col: int) -> bool:
        """
        Execute a move if valid.
        Returns True if successful, False otherwise.
        """
        if not self.is_valid_move(from_row, from_col, to_row, to_col, 
                                 arrow_row, arrow_col):
            return False
        
        # Move amazon
        piece = self.board[from_row][from_col]
        self.board[from_row][from_col] = ' '
        self.board[to_row][to_col] = piece
        
        # Place arrow
        self.board[arrow_row][arrow_col] = 'X'
        
        # Switch player
        self.current_player = 'B' if self.current_player == 'W' else 'W'
        self.move_count += 1
        
        return True
    
    def get_all_valid_moves(self) -> List[Tuple[int, int, int, int, int, int]]:
        """Get all valid moves for the current player."""
        moves = []
        
        # Find all amazons for current player
        for from_row in range(self.board_size):
            for from_col in range(self.board_size):
                if self.board[from_row][from_col] == self.current_player:
                    # Get all possible amazon moves
                    amazon_moves = self.get_queen_moves(from_row, from_col)
                    
                    for to_row, to_col in amazon_moves:
                        # Temporarily move amazon
                        piece = self.board[from_row][from_col]
                        self.board[from_row][from_col] = ' '
                        self.board[to_row][to_col] = piece
                        
                        # Get all possible arrow shots
                        arrow_shots = self.get_queen_moves(to_row, to_col)
                        
                        for arrow_row, arrow_col in arrow_shots:
                            moves.append((from_row, from_col, to_row, 
                                        to_col, arrow_row, arrow_col))
                        
                        # Restore board
                        self.board[to_row][to_col] = ' '
                        self.board[from_row][from_col] = piece
        
        return moves
    
    def is_game_over(self) -> bool:
        """Check if the game is over (current player has no moves)."""
        return len(self.get_all_valid_moves()) == 0
    
    def get_winner(self) -> Optional[str]:
        """Get the winner if game is over."""
        if self.is_game_over():
            return 'B' if self.current_player == 'W' else 'W'
        return None


class SimpleAI:
    """Simple AI that picks random valid moves."""
    
    def __init__(self, game: AmazonsGame):
        self.game = game
    
    def get_move(self) -> Optional[Tuple[int, int, int, int, int, int]]:
        """Get a move from the AI."""
        moves = self.game.get_all_valid_moves()
        if not moves:
            return None
        
        # Simple strategy: pick first available move
        # In a real implementation, this would use minimax or MCTS
        return moves[0]


def play_game_interactive():
    """Play an interactive game in the terminal."""
    print("=" * 60)
    print("ZZmazon - Game of the Amazons")
    print("=" * 60)
    print("\nLegend:")
    print("  W = White Amazon")
    print("  B = Black Amazon")
    print("  X = Burned square (arrow)")
    print("  . = Empty square")
    print("\nHow to play:")
    print("  1. Move your amazon (like a chess queen)")
    print("  2. Shoot an arrow (also like a chess queen)")
    print("  The arrow burns a square, blocking future moves")
    print("  Player who cannot move loses!")
    print()
    
    game = AmazonsGame()
    
    while not game.is_game_over():
        game.display_board()
        print(f"Current player: {'White' if game.current_player == 'W' else 'Black'}")
        print(f"Move #{game.move_count + 1}")
        
        try:
            print("\nEnter move (format: from_row from_col to_row to_col arrow_row arrow_col)")
            print("Or enter 'q' to quit, 'h' for help:")
            
            user_input = input("> ").strip()
            
            if user_input.lower() == 'q':
                print("Game ended by user.")
                return
            
            if user_input.lower() == 'h':
                print("\nValid moves for current player:")
                moves = game.get_all_valid_moves()
                if moves:
                    for i, move in enumerate(moves[:10]):  # Show first 10 moves
                        print(f"  {move[0]} {move[1]} {move[2]} {move[3]} {move[4]} {move[5]}")
                    if len(moves) > 10:
                        print(f"  ... and {len(moves) - 10} more moves")
                else:
                    print("  No valid moves available!")
                continue
            
            parts = user_input.split()
            if len(parts) != 6:
                print("Invalid input. Please enter 6 numbers.")
                continue
            
            from_row, from_col, to_row, to_col, arrow_row, arrow_col = map(int, parts)
            
            if game.make_move(from_row, from_col, to_row, to_col, arrow_row, arrow_col):
                print("Move successful!")
            else:
                print("Invalid move. Please try again.")
        
        except ValueError:
            print("Invalid input. Please enter numbers.")
        except KeyboardInterrupt:
            print("\nGame ended by user.")
            return
    
    game.display_board()
    winner = game.get_winner()
    print(f"\nGame Over! Winner: {'White' if winner == 'W' else 'Black'}")


def play_game_ai_vs_ai():
    """Play a game with AI vs AI."""
    print("=" * 60)
    print("ZZmazon - AI vs AI Demo")
    print("=" * 60)
    
    game = AmazonsGame()
    ai = SimpleAI(game)
    
    move_num = 0
    while not game.is_game_over() and move_num < 100:  # Limit to 100 moves
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


def main():
    """Main entry point."""
    if len(sys.argv) > 1 and sys.argv[1] == '--ai-demo':
        play_game_ai_vs_ai()
    else:
        play_game_interactive()


if __name__ == "__main__":
    main()
