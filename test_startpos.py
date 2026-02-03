import chess
import chess.engine
import time
import os

engine_path = r"build\IronRook.exe"

def test():
    print("Testing Start Position Probe...")
    
    if not os.path.exists(engine_path):
        print(f"Engine not found at {engine_path}")
        return

    try:
        engine = chess.engine.SimpleEngine.popen_uci(engine_path)
        
        # Start Position
        board = chess.Board()
        
        start_time = time.time()
        result = engine.play(board, chess.engine.Limit(time=1.0)) # Give it 1s, should return instantly if book
        end_time = time.time()
        
        elapsed = end_time - start_time
        print(f"Move: {result.move}")
        print(f"Time Taken: {elapsed:.4f}s")
        
        if elapsed < 0.1:
            print("Status: INSTANT (Book)")
        else:
            print("Status: CALCULATED (Not in Book)")

        engine.quit()
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test()
