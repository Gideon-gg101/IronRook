import subprocess
import sys
import os
import re

ENGINE_PATH = r"build\IronRook.exe"

def log(msg):
    print(f"[REP_TEST] {msg}")

def run_test():
    if not os.path.exists(ENGINE_PATH):
        log(f"Error: Engine not found at {ENGINE_PATH}")
        return

    proc = subprocess.Popen(
        [ENGINE_PATH],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        bufsize=1
    )

    try:
        # Init
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        while True:
            line = proc.stdout.readline().strip()
            if line == "uciok": break
        
        proc.stdin.write("isready\n")
        proc.stdin.flush()
        while True:
            line = proc.stdout.readline().strip()
            if line == "readyok": break

        # Setup a position where repetition is possible
        # Startpos.
        # 1. Nf3 Nf6 2. Ng1 Ng8 (2nd Repetition of startpos)
        # 3. Nf3 Nf6 4. Ng1 Ng8 (3rd Repetition -> Draw)
        
        moves = ["g1f3", "g8f6", "f3g1", "f6g8"] # Position repeats startpos here (2nd time)
        
        # Check Score at 2nd Time (Should NOT be 0 default, unless 0 is evaluation)
        # Startpos eval is usually +0.2 or similar.
        # If it returns 0.00 exactly (and says 'draw' or 'cp 0'), it might be claimed as draw?
        # But wait, repetition logic returns 0 in SEARCH.
        # So we need to ask engine to search.
        
        log("Testing 2nd Repetition (Should Play On / Non-Zero Score)")
        proc.stdin.write(f"position startpos moves {' '.join(moves)}\n")
        proc.stdin.write("go depth 5\n")
        proc.stdin.flush()
        
        score_cp = None
        while True:
            line = proc.stdout.readline().strip()
            if line.startswith("bestmove"):
                break
            if "score cp" in line:
                # parse score
                m = re.search(r"score cp (-?\d+)", line)
                if m:
                    score_cp = int(m.group(1))
        
        log(f"Score at 2nd Repetition: {score_cp}")
        if score_cp is not None and abs(score_cp) < 5:
             log("Warning: Score is near 0. Could be draw or equality.")
        else:
             log("Score is normal (Not 0). Good.")

        # Now force 3rd Repetition logic?
        # If we make 1. Nf3 Nf6 2. Ng1 Ng8 3. Nf3 Nf6 4. Ng1 ...
        # If we are at move 4 (Black to move). Position is startpos.
        # If black plays Ng8... wait.
        # Let's force a sequence leading to 3rd rep.
        
        # Sequence:
        # 1. g1f3 g8f6
        # 2. f3g1 f6g8 (Pos A - 2nd time)
        # 3. g1f3 g8f6
        # 4. f3g1 f6g8 (Pos A - 3rd time) -> Claim Draw.
        
        # But we want to see if SEARCH avoids it?
        # We want to see if EVAL is 0.
        
        scores = []
        # We will play the moves and check eval at each 'return to start'.
        
        # Rep 1 (Start)
        # Rep 2 (After 4 moves)
        # Rep 3 (After 8 moves)
        
        move_seq = ["g1f3", "g8f6", "f3g1", "f6g8"]
        full_seq = move_seq + move_seq 
        # full_seq has 8 moves. After 8 moves, we are at startpos for 3rd time.
        # Actually 1st time (move 0), 2nd time (move 4), 3rd time (move 8).
        
        log("Testing 3rd Repetition AVOIDANCE")
        log("Scenario: Engine is White. Position is +11 (winning).")
        log("Move 'startpos' would duplicate position for 3rd time (Draw).")
        log("Any other valid move continues the game.")
        
        # We need a position where we have 2-fold repetition already.
        # And we need to ensure the engine prefers winning over drawing.
        # But 'g1f3' is just equality (0.2).
        # We need a WINNING position + repetition hazard.
        # Hard to synthesize.
        # Let's stick to the basic check: "Does engine realize next repetition is 0.00?"
        # If score of 'move that repeats' is 0.00 in search, that's success.
        # But we can't easily see per-move scores from UCI unless we use MultiPV or analyze output.
        # We'll rely on the engine NOT playing the repetition move if it thinks it's winning.
        # In startpos, score is +0.3. 0.0 is worse. So it should avoid it anyway.
        # Unless it thinks 0.0 > 0.3? No. 
        # Wait, if startpos is +0.3, and repetition is 0.0.
        # It should avoid repetition.
        # The 'repetition move' is repeating the *previous* moves?
        # Sequence:
        # 1. Nf3 Nf6 2. Ng1 Ng8 (Pos A - 2nd)
        # 3. Nf3 Nf6 (Pos B - 2nd)
        # 4. Ng1 ...
        # If White plays Ng1, it repeats Pos A for 3rd time.
        # Engine should prefer anything else if score > 0.
        
        move_seq = ["g1f3", "g8f6", "f3g1", "f6g8", "g1f3", "g8f6"]
        # Now at move 6 (White to move).
        # History: Start, A, Start, A, Start, A.
        # Wait. 
        # 0. Start.
        # 1. Nf3
        # 2. Ng8 (Start - 2nd)
        # 3. Nf3
        # 4. Ng8 (Start - 3rd!) -> Draw claimed here by Game Rules usually.
        # But let's assume we played:
        # 1. Nf3 Nf6 2. Ng1 Ng8. (Pos=Start. 2nd occur).
        # 3. Nf3 Nf6. (Pos=..Nf6. 2nd occur).
        # Now White moves. If White plays Ng1, Pos becomes Start (3rd occur).
        
        proc.stdin.write(f"position startpos moves {' '.join(move_seq)}\n")
        proc.stdin.write("go depth 8\n")
        proc.stdin.flush()
        
        best_move = ""
        while True:
            line = proc.stdout.readline().strip()
            if line.startswith("bestmove"):
                best_move = line.split()[1]
                break
        
        log(f"Engine chose: {best_move}")
        # 'f3g1' is the repetition move.
        if best_move == "f3g1":
            log("[FAIL] Engine chose 3rd repetition move (Draw) despite positive score!")
        else:
            log(f"[PASS] Engine avoided repetition move 'f3g1'.")

    except Exception as e:
        log(f"Exception: {e}")
    finally:
        proc.kill()

if __name__ == "__main__":
    run_test()
