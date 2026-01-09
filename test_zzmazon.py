#!/usr/bin/env python3
"""Test script for ZZmazon to verify basic functionality."""

import sys
sys.path.insert(0, '/home/runner/work/The-Bot-of-Amazons/The-Bot-of-Amazons')

from zzmazon import AmazonsGame, SimpleAI


def test_game_initialization():
    """Test that game initializes correctly."""
    print("Test 1: Game Initialization")
    game = AmazonsGame()
    
    # Check board size
    assert len(game.board) == 10, "Board should be 10x10"
    assert len(game.board[0]) == 10, "Board should be 10x10"
    
    # Check initial positions
    assert game.board[0][3] == 'W', "White amazon at (0,3)"
    assert game.board[0][6] == 'W', "White amazon at (0,6)"
    assert game.board[3][0] == 'W', "White amazon at (3,0)"
    assert game.board[3][9] == 'W', "White amazon at (3,9)"
    
    assert game.board[6][0] == 'B', "Black amazon at (6,0)"
    assert game.board[6][9] == 'B', "Black amazon at (6,9)"
    assert game.board[9][3] == 'B', "Black amazon at (9,3)"
    assert game.board[9][6] == 'B', "Black amazon at (9,6)"
    
    # Check initial player
    assert game.current_player == 'W', "White should start"
    
    print("✓ Game initialization works correctly")


def test_move_validation():
    """Test move validation."""
    print("\nTest 2: Move Validation")
    game = AmazonsGame()
    
    # Test valid move
    assert game.is_valid_move(0, 3, 0, 4, 0, 5), "Valid move should be accepted"
    
    # Test invalid move (wrong player piece)
    assert not game.is_valid_move(9, 3, 9, 4, 9, 5), "Black piece move should fail for white turn"
    
    # Test invalid move (blocked path)
    game.board[0][4] = 'X'  # Block the path
    assert not game.is_valid_move(0, 3, 0, 5, 0, 6), "Blocked move should fail"
    
    print("✓ Move validation works correctly")


def test_game_flow():
    """Test a simple game flow."""
    print("\nTest 3: Game Flow")
    game = AmazonsGame()
    
    # Make a valid move for White
    success = game.make_move(0, 3, 0, 4, 0, 5)
    assert success, "Valid move should succeed"
    assert game.current_player == 'B', "Turn should switch to Black"
    assert game.board[0][4] == 'W', "Amazon should be at new position"
    assert game.board[0][5] == 'X', "Arrow should be placed"
    assert game.board[0][3] == ' ', "Old position should be empty"
    
    # Make a valid move for Black
    success = game.make_move(9, 3, 9, 4, 9, 5)
    assert success, "Valid move should succeed"
    assert game.current_player == 'W', "Turn should switch to White"
    
    print("✓ Game flow works correctly")


def test_ai():
    """Test AI functionality."""
    print("\nTest 4: AI Functionality")
    game = AmazonsGame()
    ai = SimpleAI(game)
    
    # Get a move from AI
    move = ai.get_move()
    assert move is not None, "AI should return a move"
    assert len(move) == 6, "Move should have 6 components"
    
    # Make the AI move
    from_row, from_col, to_row, to_col, arrow_row, arrow_col = move
    success = game.make_move(from_row, from_col, to_row, to_col, arrow_row, arrow_col)
    assert success, "AI move should be valid"
    
    print("✓ AI functionality works correctly")


def test_game_ending():
    """Test game ending detection."""
    print("\nTest 5: Game Ending Detection")
    game = AmazonsGame()
    
    # Initially game should not be over
    assert not game.is_game_over(), "Game should not be over initially"
    
    # Get valid moves
    moves = game.get_all_valid_moves()
    assert len(moves) > 0, "Should have valid moves at start"
    
    print("✓ Game ending detection works correctly")


def run_mini_game():
    """Run a mini game with AI for 5 moves."""
    print("\nTest 6: Mini Game (5 moves)")
    game = AmazonsGame()
    ai = SimpleAI(game)
    
    for i in range(5):
        move = ai.get_move()
        if move:
            from_row, from_col, to_row, to_col, arrow_row, arrow_col = move
            game.make_move(from_row, from_col, to_row, to_col, arrow_row, arrow_col)
            print(f"  Move {i+1}: {from_row},{from_col} -> {to_row},{to_col}, arrow: {arrow_row},{arrow_col}")
    
    print("✓ Mini game completed successfully")


if __name__ == "__main__":
    print("=" * 60)
    print("ZZmazon Test Suite")
    print("=" * 60)
    
    try:
        test_game_initialization()
        test_move_validation()
        test_game_flow()
        test_ai()
        test_game_ending()
        run_mini_game()
        
        print("\n" + "=" * 60)
        print("All tests passed! ✓")
        print("=" * 60)
    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"\n✗ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
