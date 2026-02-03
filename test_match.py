import chess
import chess.engine
import chess.pgn
import sys

def main():
    engine_path = r"build\IronRook.exe"
    
    print(f"Starting match between IronRook vs IronRook...")
    
    try:
        # Create two engine instances
        p1 = chess.engine.SimpleEngine.popen_uci(engine_path)
        p2 = chess.engine.SimpleEngine.popen_uci(engine_path)
        
        # Configure them?
        # p1.configure({"Hash": 16})
        
        board = chess.Board()
        game = chess.pgn.Game()
        game.headers["Event"] = "Test Match"
        game.headers["White"] = "IronRook A"
        game.headers["Black"] = "IronRook B"
        
        node = game
        
        while not board.is_game_over():
            # Side to move
            engine = p1 if board.turn == chess.WHITE else p2
            
            # Search
            limit = chess.engine.Limit(time=0.1) # Fast game
            result = engine.play(board, limit)
            
            board.push(result.move)
            node = node.add_variation(result.move)
            
            print(f"{board.fullmove_number}. {result.move.uci()} ({board.fen()})")
            
        print(f"Game Over. Result: {board.result()}")
        game.headers["Result"] = board.result()
        
        print(game, file=sys.stdout)
        
        p1.quit()
        p2.quit()
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    main()
