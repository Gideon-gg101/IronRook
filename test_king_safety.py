import subprocess
import sys
import os
import re

ENGINE_PATH = r"build\IronRook.exe"

def log(msg):
    print(f"[KING_TEST] {msg}")

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

        # Scenario: Opening/Middlegame where e1e2 is legal but bad.
        # Position after 1. e4 e5 2. d4 exd4 3. Qxd4 Nc6 4. Qe3.
        # Board: r1bqkbnr/pppp1ppp/2n5/8/8/4Q3/PPP1PPPP/RNB1KBNR b KQkq - 2 4
        # White to move:
        # e1e2 blocks bishop, queen? No queen on e3.
        # But e2 is exposed.
        # Let's take a simpler position.
        # Startpos. 1. e3 e5.
        # Position: rnbqkbnr/pppp1ppp/8/4p3/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 2
        # White can play Ke2.
        # Previously, Ke2 might have scored +40 cp better than Ke1 due to PST?
        # We want to check evaluate score difference between Ke1 and Ke2.
        # But we can only see search score of Ke2 if it's best?
        # Or use 'eval' command if available?
        # 'eval' is usually a debug command. Check uci.cpp.
        
        # We can set position with Ke2 and check static eval?
        # Position A: Ke1 (Startpos after e3 e5)
        # Position B: Ke2 (Startpos after e3 e5 Ke2)
        
        # Eval A should be ~+0.2
        # Eval B should be < Eval A (significantly worse).
        # Previously: Ke2 gained +47 cp from PST.
        # So Eval B might have been > Eval A!
        
        log("Comparing Eval of Ke1 vs Ke2 (Bongcloud test)")
        
        # Pos A (Ke1): 
        fen_a = "rnbqkbnr/pppp1ppp/8/4p3/8/4P3/PPPP1PPP/RNBQKBNR w KQkq - 0 2"
        proc.stdin.write(f"position fen {fen_a}\n")
        proc.stdin.write("eval\n") # Assuming 'eval' command prints score
        proc.stdin.flush()
        
        while True:
            line = proc.stdout.readline().strip()
            # Engine output: Total Score: 0 cp
            if "Total Score:" in line:
                 m = re.search(r"Total Score:\s*(-?\d+)", line)
                 if m: 
                     score_cp = int(m.group(1))
                     # Convert CP to float (e.g. 0.25)
                     score_a = score_cp / 100.0
                 break
            if "Unknown command" in line:
                 log("Engine does not support 'eval' command?")
                 break
            if not line: break 
            
        log(f"Score Ke1 (Pos A): {score_a}")

        # Pos B (Ke2):
        fen_b = "rnbqkbnr/pppp1ppp/8/4p3/8/4P3/PPPPKPPP/RNBQ1BNR b kq - 1 2"
        proc.stdin.write(f"position fen {fen_b}\n")
        proc.stdin.write("eval\n") 
        proc.stdin.flush()
        
        score_b = None
        while True:
             line = proc.stdout.readline().strip()
             if "Total Score:" in line:
                 m = re.search(r"Total Score:\s*(-?\d+)", line)
                 if m: 
                     score_cp = int(m.group(1))
                     score_b = score_cp / 100.0
                 break
        
        log(f"Score Ke2 (Pos B): {score_b}")
        
        if score_a is not None and score_b is not None:
             diff = score_a - score_b
             log(f"Advantage of Ke1 over Ke2: {diff}")
             if diff > 0.5: # Should be at least 50cp better
                 log("[PASS] Ke1 is significantly better than Ke2.")
             elif diff < 0:
                 log("[FAIL] Ke2 is evaluated as BETTER than Ke1!")
             else:
                 log("[WARN] Ke1 is better but margin is small.")
        else:
             log("Could not parse eval scores.")

    except Exception as e:
        log(f"Exception: {e}")
    finally:
        proc.kill()

if __name__ == "__main__":
    run_test()
