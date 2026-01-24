import subprocess
import time
import os
import sys

# Usage: python test_regression.py <engine1> <engine2> <games> <time_ms>

def log(msg):
    print(f"[REGRESSION] {msg}")

def run_game(e1_path, e2_path, time_ms):
    # Simplified match: 1 game
    # We use similar logic to test_match.py but with two separate processes
    
    p1 = subprocess.Popen([e1_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    p2 = subprocess.Popen([e2_path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
    
    # Init
    for p in [p1, p2]:
        p.stdin.write("uci\nisready\n")
        p.stdin.flush()
        while True:
            line = p.stdout.readline().strip()
            if line == "readyok": break
            
    moves = []
    wtime = time_ms
    btime = time_ms
    
    turn = 0 # 0=White(p1), 1=Black(p2)
    result = "*"
    
    try:
        while True:
            current_p = p1 if turn % 2 == 0 else p2
            cmd = "position startpos"
            if moves:
                cmd += " moves " + " ".join(moves)
            current_p.stdin.write(cmd + "\n")
            
            start = time.time()
            current_p.stdin.write(f"go wtime {wtime} btime {btime}\n")
            current_p.stdin.flush()
            
            best_move = None
            while True:
                line = current_p.stdout.readline()
                if not line: break
                line = line.strip()
                if line.startswith("bestmove"):
                    best_move = line.split()[1]
                    break
            
            end = time.time()
            elapsed = int((end - start) * 1000)
            
            if turn % 2 == 0: wtime -= elapsed
            else: btime -= elapsed
            
            if wtime < 0 or btime < 0:
                result = "0-1" if wtime < 0 else "1-0"
                break
                
            if not best_move or best_move == "(none)":
                result = "1/2-1/2" # Draw or stalemate
                break
                
            moves.append(best_move)
            turn += 1
            if turn > 300: # Draw by move count
                result = "1/2-1/2"
                break
    except:
        pass
    finally:
        p1.kill()
        p2.kill()
        
    return result

def main():
    if len(sys.argv) < 5:
        print("Usage: python test_regression.py <engine1> <engine2> <games> <time_ms>")
        return

    e1 = sys.argv[1]
    e2 = sys.argv[2]
    games = int(sys.argv[3])
    time_ms = int(sys.argv[4])
    
    wins = 0
    losses = 0
    draws = 0
    
    for i in range(games):
        # Alternate sides
        swap = i % 2 == 1
        res = run_game(e2 if swap else e1, e1 if swap else e2, time_ms)
        
        if res == "1-0":
            if not swap: wins += 1
            else: losses += 1
        elif res == "0-1":
            if not swap: losses += 1
            else: wins += 1
        else:
            draws += 1
            
        log(f"Game {i+1}/{games}: {res} (Score: +{wins} -{losses} ={draws})")
        
    log(f"Final Score: {wins}-{losses}-{draws}")

if __name__ == "__main__":
    main()
