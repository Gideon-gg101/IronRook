import sys
import re
import os

def parse_result(result_str):
    if "1-0" in result_str: return "1.0"
    if "0-1" in result_str: return "0.0"
    if "1/2-1/2" in result_str: return "0.5"
    return "0.5" # Default to draw

def main():
    if len(sys.argv) < 3:
        print("Usage: python pgn_to_epd.py <input.pgn> <output.epd>")
        return

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    if not os.path.exists(input_file):
        print(f"Error: Input file {input_file} not found.")
        return

    print(f"Converting {input_file} to {output_file}...")

    # We use python-chess if available, otherwise raw parsing (limited)
    try:
        import chess.pgn
        
        with open(input_file, 'r') as pgn, open(output_file, 'w') as epd:
            count = 0
            while True:
                game = chess.pgn.read_game(pgn)
                if game is None: break

                result = parse_result(game.headers.get("Result", "*"))
                board = game.board()

                # Iterate moves
                for move in game.mainline_moves():
                    board.push(move)
                    # Skip initial opening phase? (Optional)
                    # if board.fullmove_number > 8: 
                    epd.write(f"{board.fen()} | {result}\n")
                
                count += 1
                if count % 100 == 0:
                    print(f"Processed {count} games...", end='\r')
            
            print(f"\nFinished! Processed {count} games.")
            
    except ImportError:
        print("Error: 'chess' library not found. Please install it using:")
        print("pip install python-chess")
        print("Raw parsing logic is too complex for robust PGN handling without a library.")

if __name__ == "__main__":
    main()
