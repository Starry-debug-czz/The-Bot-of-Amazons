#!/usr/bin/env python3
"""
Example: Botzone adapter for ZZmazon
This shows how to adapt ZZmazon for Botzone platform integration.
"""

import json
import sys
sys.path.insert(0, '..')

from zzmazon import AmazonsGame


def parse_botzone_input():
    """Parse input from Botzone in JSON format."""
    try:
        data = json.loads(input())
        return data
    except:
        return None


def format_botzone_output(move):
    """Format move for Botzone output."""
    from_row, from_col, to_row, to_col, arrow_row, arrow_col = move
    output = {
        "response": {
            "x0": from_row,
            "y0": from_col,
            "x1": to_row,
            "y1": to_col,
            "x2": arrow_row,
            "y2": arrow_col
        }
    }
    return json.dumps(output)


def botzone_main():
    """Main function for Botzone integration."""
    game = AmazonsGame()
    
    # Read game history from Botzone
    data = parse_botzone_input()
    
    if data and "requests" in data:
        # Replay the game history
        for request in data["requests"]:
            if request:  # Skip empty first request
                x0, y0 = request.get("x0", 0), request.get("y0", 0)
                x1, y1 = request.get("x1", 0), request.get("y1", 0)
                x2, y2 = request.get("x2", 0), request.get("y2", 0)
                game.make_move(x0, y0, x1, y1, x2, y2)
        
        # Also apply opponent's last move if present
        if "response" in data and data["response"]:
            resp = data["response"]
            x0, y0 = resp.get("x0", 0), resp.get("y0", 0)
            x1, y1 = resp.get("x1", 0), resp.get("y1", 0)
            x2, y2 = resp.get("x2", 0), resp.get("y2", 0)
            game.make_move(x0, y0, x1, y1, x2, y2)
    
    # Generate our move
    from zzmazon import SimpleAI
    ai = SimpleAI(game)
    move = ai.get_move()
    
    if move:
        print(format_botzone_output(move))
    else:
        # No valid moves - output a dummy move
        print(json.dumps({"response": {"x0": 0, "y0": 0, "x1": 0, "y1": 0, "x2": 0, "y2": 0}}))


if __name__ == "__main__":
    botzone_main()
