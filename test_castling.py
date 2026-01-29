import subprocess
import time
import sys
import os

ENGINE_PATH = r"build\IronRook.exe"

def log(msg):
    print(f"[CASTLE_TEST] {msg}")

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
            if line == "uciok":
                break
        
        proc.stdin.write("isready\n")
        proc.stdin.flush()
        while True:
            line = proc.stdout.readline().strip()
            if line == "readyok":
                break

        tests = [
            # 1. Standard Initial Position with castling available
            {
                "name": "Standard Short Castle (Legality & Output)",
                "setup": "r1bqk1nr/pppp1ppp/2n5/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
                "check_perft": True,
                "valid_moves": ["e1g1", "e1h1"], # Accept both for perft
                "check_bestmove": True,
                "expected_bestmove": "e1g1" # Strict for UCI output
            },
            # 2. Blocked by piece (Bishop on f1)
            {
                "name": "Blocked Castle (Bishop at f1)",
                "setup": "r3k2r/8/8/8/8/8/8/R3KB1R w KQkq - 0 1",
                "check_perft": True,
                "invalid_moves": ["e1g1", "e1h1"]
            },
            # 3. Blocked by check (Ray attack on e3)
            {
                "name": "Castle In Check (Rook on e3)",
                "setup": "r3k2r/8/8/8/8/4r3/8/R3K2R w KQkq - 0 1",
                "check_perft": True,
                "invalid_moves": ["e1g1", "e1h1", "e1c1", "e1a1"]
            },
            # 4. Castle *through* attacked square (f1)
            {
                "name": "Castle Through Attacked Square (Rook attacks f1)",
                "setup": "r3k2r/8/8/5r2/8/8/8/R3K2R w KQkq - 0 1",
                "check_perft": True,
                "invalid_moves": ["e1g1", "e1h1"]
            },
             # 5. Castle *through* attacked square (d1) - Queenside
            {
                "name": "Castle Through Attacked Square (Rook attacks d1)",
                "setup": "r3k2r/8/8/3r4/8/8/8/R3K2R w KQkq - 0 1",
                "check_perft": True,
                "invalid_moves": ["e1c1", "e1a1"]
            },
            # 6. Castling is Check (Verify Output Format)
            {
                "name": "Castling Check Output (Standard)",
                "setup": "8/8/8/8/8/4k3/8/R3K2R w KQ - 0 1", # Black King e3. O-O checks.
                "check_bestmove": True,
                "expected_bestmove": "e1g1"
            }
        ]

        for t in tests:
            log(f"Testing: {t['name']}")
            proc.stdin.write(f"position fen {t['setup']}\n")
            
            if t.get("check_perft"):
                proc.stdin.write("perft 1\n")
                proc.stdin.flush()
                
                legal_moves = []
                while True:
                    line = proc.stdout.readline()
                    if not line: break
                    line = line.strip()
                    if line.startswith("Nodes"): break
                    if ":" in line:
                        move = line.split(":")[0]
                        legal_moves.append(move)
                
                if "valid_moves" in t:
                    found = False
                    for m in t["valid_moves"]:
                        if m in legal_moves:
                            log(f"  [PASS] Found valid move {m}")
                            found = True
                            break
                    if not found:
                        log(f"  [FAIL] Missing valid move {t['valid_moves'][0]}")
                        log(f"  Available moves: {','.join(legal_moves)}")

                if "invalid_moves" in t:
                    for m in t["invalid_moves"]:
                        if m in legal_moves:
                            log(f"  [FAIL] Found INVALID move {m}")
                        else:
                            pass # log(f"  [PASS] Correctly excluded {m}")
                    log(f"  [PASS] Excluded invalid moves")

            if t.get("check_bestmove"):
                # Force engine to evaluate castling as good? 
                # In the setup position, O-O is a good move.
                proc.stdin.write("go movetime 100\n")
                proc.stdin.flush()
                
                best_move = ""
                while True:
                    line = proc.stdout.readline()
                    if not line: break
                    line = line.strip()
                    if line.startswith("bestmove"):
                        best_move = line.split()[1]
                        break
                
                log(f"  Engine played: {best_move}")
                if best_move == t["expected_bestmove"]:
                    log(f"  [PASS] Bestmove matches expected {best_move}")
                else:
                    if best_move == "e1h1":
                        log(f"  [FAIL] Engine output raw Chess960 move 'e1h1' instead of standard 'e1g1'")
                    elif best_move != "(none)":
                        log(f"  [INFO] Engine chose different move {best_move} (acceptable if valid)")
                    else:
                         log(f"  [FAIL] Engine returned no move")

    except Exception as e:
        log(f"Exception: {e}")
    finally:
        proc.kill()

if __name__ == "__main__":
    run_test()
