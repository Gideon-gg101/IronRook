import os
import sys
import chess.pgn

# Settings
GAMES_DIR = "Games"
OUTPUT_FILE = "Games/tuning_data_all.epd"

def parse_result(result_str):
    if "1-0" in result_str: return "1.0"
    if "0-1" in result_str: return "0.0"
    if "1/2-1/2" in result_str: return "0.5"
    return "0.5" # Default to draw

def main():
    if not os.path.exists(GAMES_DIR):
        print(f"Error: Directory {GAMES_DIR} not found.")
        return

    pgn_files = [f for f in os.listdir(GAMES_DIR) if f.endswith(".pgn")]
    if not pgn_files:
        print("No PGN files found in Games directory.")
        return

    print(f"Found {len(pgn_files)} PGN files. Converting to {OUTPUT_FILE}...")

    total_games = 0
    total_positions = 0

    with open(OUTPUT_FILE, 'w') as epd_out:
        for pgn_filename in pgn_files:
            pgn_path = os.path.join(GAMES_DIR, pgn_filename)
            print(f"Processing {pgn_filename}...")
            
            with open(pgn_path, 'r', encoding='utf-8', errors='ignore') as f:
                while True:
                    try:
                        game = chess.pgn.read_game(f)
                    except Exception as e:
                        print(f"  Error reading game in {pgn_filename}: {e}")
                        continue
                        
                    if game is None:
                        break

                    result = parse_result(game.headers.get("Result", "*"))
                    board = game.board()
                    
                    # We usually skip the first few moves of an opening (book moves)
                    # to focus on tuning the engine's middle/endgame evaluation.
                    move_count = 0
                    for move in game.mainline_moves():
                        board.push(move)
                        move_count += 1
                        
                        # Only convert if the game isn't over and it's past move 8
                        if not board.is_game_over() and move_count > 16: # 8 full moves
                             epd_out.write(f"{board.fen()} | {result}\n")
                             total_positions += 1
                    
                    total_games += 1
                    if total_games % 500 == 0:
                        print(f"  Processed {total_games} games ({total_positions} positions)...")

    print(f"\nSuccess!")
    print(f"Total Games: {total_games}")
    print(f"Total Positions: {total_positions}")
    print(f"Dataset saved to: {OUTPUT_FILE}")

if __name__ == "__main__":
    main()
