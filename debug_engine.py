import subprocess
import sys
import time

ENGINE = r"Prometheus_v1.1_Final.exe"

def run():
    print(f"Starting {ENGINE}...")
    p = subprocess.Popen([ENGINE], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1)
    
    def send(cmd):
        print(f"-> {cmd}")
        p.stdin.write(cmd + "\n")
        p.stdin.flush()
        
    def read_loop(timeout=2.0):
        start = time.time()
        while time.time() - start < timeout:
            line = p.stdout.readline()
            if line:
                print(f"<- {line.strip()}")
            else:
                if p.poll() is not None:
                    print(f"Engine exited with code {p.returncode}")
                    return False
                time.sleep(0.01)
        return True

    # send("uci")
    # if not read_loop(1.0): return
    
    # send("isready")
    # if not read_loop(1.0): return
    
    # send("position startpos")
    # if not read_loop(0.5): return
    
    send("bench")
    if not read_loop(5.0): return
    
    send("quit")
    p.wait()

run()
