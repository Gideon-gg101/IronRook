import subprocess
import time
import sys
import os

ENGINE_PATH = r"build\Prometheus.exe"

def log(msg):
    print(f"[TEST] {msg}")

def read_response(process):
    lines = []
    while True:
        line = process.stdout.readline()
        if not line:
            break
        line = line.strip()
        # log(f"Engine: {line}")
        if line == 'readyok':
            break
        if line.startswith('bestmove'):
            return line
    return None

def get_bestmove(process, moves_history, wtime, btime):
    position_cmd = f"position startpos moves {' '.join(moves_history)}\n"
    process.stdin.write(position_cmd)
    process.stdin.flush()
    
    go_cmd = f"go wtime {wtime} btime {btime}\n"
    process.stdin.write(go_cmd)
    process.stdin.flush()
    
    while True:
        line = process.stdout.readline()
        if not line:
            break
        line = line.strip()
        if line.startswith('bestmove'):
            return line.split()[1]
        
def run_match():
    if not os.path.exists(ENGINE_PATH):
        log(f"Error: Engine not found at {ENGINE_PATH}")
        return

    log(f"Starting match with {ENGINE_PATH}...")
    
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
            # print(f"Engine Init: {line}")
            if line == "uciok":
                break
                
        proc.stdin.write("isready\n")
        proc.stdin.flush()
        while True:
            line = proc.stdout.readline().strip()
            if line == "readyok":
                break

        moves = []
        wtime = 60000
        btime = 60000
        start_time = time.time()
        
        turn = 0 # 0=White, 1=Black
        
        # Play for up to 60 seconds of real time or ~100 moves
        while time.time() - start_time < 65: # Buffer for 1m game
            
            # Send position
            cmd_pos = "position startpos"
            if moves:
                cmd_pos += " moves " + " ".join(moves)
            proc.stdin.write(cmd_pos + "\n")
            
            # Search
            move_start = time.time()
            proc.stdin.write(f"go wtime {wtime} btime {btime}\n")
            proc.stdin.flush()
            
            best_move = None
            while True:
                line = proc.stdout.readline()
                if not line:
                    break
                line = line.strip()
                
                if line.startswith("bestmove"):
                    best_move = line.split()[1]
                    break
            
            move_end = time.time()
            elapsed = int((move_end - move_start) * 1000)
            
            if turn % 2 == 0:
                wtime -= elapsed
            else:
                btime -= elapsed
                
            if not best_move or best_move == "(none)":
                log("Game Over (No move)")
                break
                
            moves.append(best_move)
            log(f"Ply {turn+1}: {best_move} ({elapsed}ms)")
            
            turn += 1
            if turn > 200: # Safety break
                break
                
        log("Match completed successfully.")
        
    except Exception as e:
        log(f"Exception: {e}")
    finally:
        proc.kill()

if __name__ == "__main__":
    run_match()
