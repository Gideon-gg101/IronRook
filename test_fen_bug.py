import chess
import chess.engine
import os

engine_path = r"build\IronRook.exe"

def test():
    print("Testing FEN Bug...")
    if not os.path.exists(engine_path):
        print(f"Engine not found at {engine_path}")
        return

    # 1. e4 position (Black to move)
    fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"
    
    try:
        engine = chess.engine.SimpleEngine.popen_uci(engine_path)
        board = chess.Board(fen)
        print(f"Position: {fen}")
        print(f"Side to move: {board.turn} (False=Black, True=White)")
        
        # Verify engine behavior
        result = engine.play(board, chess.engine.Limit(depth=4))
        print(f"Best Move: {result.move}")
        
        # Check legality
        if result.move not in board.legal_moves:
             print(f"FATAL: Engine returned illegal move {result.move}!")
        else:
             print("Move is legal.")

        engine.quit()

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test()
