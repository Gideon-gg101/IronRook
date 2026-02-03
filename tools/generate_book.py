import chess
import chess.engine
import random
import sys
import os

# Configuration
ENGINE_PATH = r"build\IronRook.exe"
TARGET_PLY = 8       # 4 moves per side
EVAL_TIME = 0.1      # Seconds per position to verify
MAX_SCORE = 150      # Centipawns (1.5 pawns)
MIN_SCORE = -150
ATTEMPTS = 500       # How many positions to try generating

def generate_position(board, ply):
    """Play random moves to reach depth."""
    for _ in range(ply):
        if board.is_game_over(): 
            return False
        moves = list(board.legal_moves)
        if not moves: 
            return False
        move = random.choice(moves)
        board.push(move)
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_book.py <output.epd> [count]")
        return

    output_file = sys.argv[1]
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 100

    print(f"Generating {count} book positions (Ply={TARGET_PLY})...")
    
    unique_fens = set()
    
    # Load Engine
    try:
        engine = chess.engine.SimpleEngine.popen_uci(ENGINE_PATH)
    except Exception as e:
        print(f"Error loading engine: {e}")
        return

    generated = 0
    tried = 0

    try:
        while generated < count:
            tried += 1
            board = chess.Board()
            
            # 1. Random Walk
            if not generate_position(board, TARGET_PLY):
                continue
            
            fen = board.fen()
            
            # Deduplicate
            if fen in unique_fens:
                continue

            # 2. Engine Evaluation (Filter Blunders)
            info = engine.analyse(board, chess.engine.Limit(time=EVAL_TIME))
            score = info["score"].relative.score(mate_score=10000)

            # Check Bounds
            if score is not None and MIN_SCORE <= score <= MAX_SCORE:
                unique_fens.add(fen)
                generated += 1
                print(f"[{generated}/{count}] Score: {score} cp  | {fen}")
            else:
                pass 
                # print(f"Skipped (Score {score})")

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        engine.quit()

    # Save
    with open(output_file, "w") as f:
        for fen in unique_fens:
            f.write(fen + "\n")
    
    print(f"\nSaved {len(unique_fens)} positions to {output_file}")

if __name__ == "__main__":
    main()
