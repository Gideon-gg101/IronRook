import sys
import chess
import chess.pgn
import chess.engine
import random
import time

def play_game(engine_path, time_limit=0.1):
    # Using python-chess engine wrapper (SimpleEngine for sync usage)
    try:
        engine1 = chess.engine.SimpleEngine.popen_uci(engine_path)
        engine2 = chess.engine.SimpleEngine.popen_uci(engine_path)
    except Exception as e:
        print(f"Failed to start engine: {e}")
        return None

    board = chess.Board()
    game = chess.pgn.Game()
    game.headers["Event"] = "Self Play"
    game.headers["Site"] = "GitHub Actions"
    game.headers["Date"] = time.strftime("%Y.%m.%d")
    game.headers["Round"] = "1"
    game.headers["White"] = "IronRook"
    game.headers["Black"] = "IronRook"

    node = game

    # Opening Randomization: Play 8 random moves (4 ply each side) to ensure variety
    for _ in range(8):
        if board.is_game_over(): break
        legal_moves = list(board.legal_moves)
        if not legal_moves: break
        random_move = random.choice(legal_moves)
        board.push(random_move)
        node = node.add_variation(random_move)

    while not board.is_game_over():
        # Select engine based on turn
        engine = engine1 if board.turn == chess.WHITE else engine2
        
        try:
            # Randomize opening slightly? 
            # Engine usually has some randomness if MultiPV or just simple search noise.
            # We enforce a small movetime.
            limit = chess.engine.Limit(time=time_limit)
            result = engine.play(board, limit)
            
            if result.move is None:
                print("Engine returned no move!")
                break
                
            board.push(result.move)
            node = node.add_variation(result.move)
            
        except Exception as e:
            print(f"Error during play: {e}")
            break

    # Outcome
    game.headers["Result"] = board.result()
    
    engine1.quit()
    engine2.quit()
    
    return game

def main():
    if len(sys.argv) < 4:
        print("Usage: python selfplay.py <engine_path> <output.pgn> <num_games>")
        return

    engine_path = sys.argv[1]
    output_file = sys.argv[2]
    num_games = int(sys.argv[3])

    print(f"Starting self-play: {num_games} games using {engine_path}")

    with open(output_file, "w", encoding="utf-8") as f:
        for i in range(num_games):
            print(f"Playing game {i+1}/{num_games}...", end='\r')
            game = play_game(engine_path)
            if game:
                exporter = chess.pgn.FileExporter(f)
                game.accept(exporter)
                f.flush() # Ensure write
            else:
                print(f"Game {i+1} failed.")
    
    print(f"\nCompleted. Saved to {output_file}")

if __name__ == "__main__":
    main()
