import sys
import chess
import chess.pgn
import chess.engine
import random
import time

def play_game(engine1, engine2, time_limit=0.1):
    board = chess.Board()
    game = chess.pgn.Game()
    game.headers["Event"] = "Self Play"
    game.headers["Site"] = "GitHub Actions"
    game.headers["Date"] = time.strftime("%Y.%m.%d")
    game.headers["Round"] = "1"
    game.headers["White"] = "IronRook"
    game.headers["Black"] = "IronRook"

    node = game

    # New Game Signal (Important when reusing engine)
    # python-chess manages ucinewgame automatically when analyzing usually, 
    # but for play() it assumes continuing matching unless we reset?
    # SimpleEngine sends ucinewgame on first use usually.
    # We should ensure engines are ready.
    # Actually, SimpleEngine doesn't strictly expose "newgame".
    # But analyzing new position sends "position ...".
    # Engines should handle it. IronRook handles "ucinewgame" to clear hash.
    # We can send it explicitly if accessible, but SimpleEngine wraps UCI protocol.
    # Assuming it works fine as it just sends "position" commands.

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
    return game

def main():
    if len(sys.argv) < 4:
        print("Usage: python selfplay.py <engine_path> <output.pgn> <num_games>")
        return

    engine_path = sys.argv[1]
    output_file = sys.argv[2]
    num_games = int(sys.argv[3])

    print(f"Starting self-play: {num_games} games using {engine_path}")

    # Initialize engines ONCE
    try:
        engine1 = chess.engine.SimpleEngine.popen_uci(engine_path)
        engine2 = chess.engine.SimpleEngine.popen_uci(engine_path)
    except Exception as e:
        print(f"Failed to start engines: {e}")
        return

    try:
        with open(output_file, "w", encoding="utf-8") as f:
            for i in range(num_games):
                if (i + 1) % 10 == 0:
                    print(f"Playing game {i+1}/{num_games}...")
                game = play_game(engine1, engine2)
                if game:
                    exporter = chess.pgn.FileExporter(f)
                    game.accept(exporter)
                    f.flush()
                else:
                    print(f"Game {i+1} failed.")
    finally:
        engine1.quit()
        engine2.quit()
    
    print(f"\nCompleted. Saved to {output_file}")

if __name__ == "__main__":
    main()
