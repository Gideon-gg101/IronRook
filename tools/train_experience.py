import sys
import os
import time
import chess
import chess.engine

# Configuration
DEFAULT_ENGINE = r"build\Release\IronRook.exe" if os.name == 'nt' else "./build/IronRook"
DEFAULT_DEPTH = 10

def main():
    if len(sys.argv) < 3:
        print("Usage: python train_experience.py <book.epd> <output_exp_file> [engine_path]")
        return

    book_file = sys.argv[1]
    exp_file = sys.argv[2]
    engine_path = sys.argv[3] if len(sys.argv) > 3 else DEFAULT_ENGINE
    
    # Fix paths
    exp_file = os.path.abspath(exp_file)
    if not os.path.exists(engine_path):
        # try checking build/IronRook.exe directly
        alt_path1 = r"build\IronRook.exe"
        alt_path2 = r"build\Release\IronRook.exe"
        if os.path.exists(alt_path1):
            engine_path = alt_path1
        elif os.path.exists(alt_path2):
            engine_path = alt_path2
        else:
            print(f"Error: Engine not found at {engine_path}")
            return

    print(f"Training Experience from {book_file}")
    print(f"Target: {exp_file}")
    print(f"Engine: {engine_path}")

    # Start Engine
    try:
        # We use Popen directly or SimpleEngine? 
        # SimpleEngine is easier but we need to ensure options are set.
        engine = chess.engine.SimpleEngine.popen_uci(engine_path)
    except Exception as e:
        print(f"Failed to launch engine: {e}")
        return

    # Configure Experience
    try:
        # Check if option exists
        if "ExperiencePath" in engine.options:
            engine.configure({"ExperiencePath": exp_file})
        else:
            print("Error: Engine does not support 'ExperiencePath' option.")
            engine.quit()
            return
            
        print("ExperiencePath configured.")
    except Exception as e:
        print(f"Error configuring engine: {e}")
        engine.quit()
        return

    # Check if book exists
    if not os.path.exists(book_file):
        print(f"Book file not found: {book_file}")
        engine.quit()
        return
        
    positions = []
    with open(book_file, 'r') as f:
        for line in f:
            fen = line.strip().split('|')[0].strip() # Handle "FEN | Result" or just "FEN"
            if fen:
                positions.append(fen)
    
    print(f"Loaded {len(positions)} positions.")
    
    start_time = time.time()
    
    try:
        for i, fen in enumerate(positions):
            board = chess.Board(fen)
            
            # Run Search
            # limit depth to slightly above min recording depth (8)
            limit = chess.engine.Limit(depth=DEFAULT_DEPTH)
            
            # Run Search (Blocking)
            # engine.play waits for the limit (depth=10) and ensures clean state
            result = engine.play(board, limit)
            
            # Progress
            if (i+1) % 5 == 0:
                elapsed = time.time() - start_time
                print(f"[{i+1}/{len(positions)}] Processed. Time: {elapsed:.1f}s", end='\r')
                
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        print("\nSending quit command (saves experience)...")
        engine.quit() # Should trigger save
        
    print("Done.")

if __name__ == "__main__":
    main()
