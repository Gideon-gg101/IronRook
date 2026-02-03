import chess
import chess.engine
import time

def main():
    engine_path = r"build\IronRook.exe"
    book_path = "IronBook.exp"
    
    # Position from start (likely in book as it's the first thing learned)
    # Or valid FEN from 8moves_v3.epd
    # 8moves_v3 line 1: rn2kbnr/pp2pppp/3p4/q1p5/4P1b1/1P1P4/P1PN1PPP/R1BQKBNR w KQkq - 3 5 
    # (Actually line 1 of my_book.epd, let's grab a standard one)
    # Let's try startpos
    # Position from my_book.epd line 1:
    fen = "rn2kbnr/pp2pppp/3p4/q1p5/4P1b1/1P1P4/P1PN1PPP/R1BQKBNR w KQkq - 3 5"
    
    print(f"Testing Book Probe...")
    
    try:
        engine = chess.engine.SimpleEngine.popen_uci(engine_path)
        
        # Configure Book (Removed manual config to test default)
        # engine.configure({"ExperiencePath": book_path})
        print(f"Using default book configuration...")
        
        board = chess.Board(fen)
        
        start = time.time()
        result = engine.play(board, chess.engine.Limit(time=1.0)) # Give it 1s, should take 0.0s
        elapsed = time.time() - start
        
        print(f"Move: {result.move}")
        print(f"Time Taken: {elapsed:.4f}s")
        
        if elapsed < 0.1:
            print("✅ INSTANT MOVE - Book is WORKING!")
        else:
            print("❌ SLOW MOVE - Book MISS or Engine Search")
            
        engine.quit()
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
