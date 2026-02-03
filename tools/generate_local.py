import os
import sys
import time
import subprocess

# Settings
ENGINE_PATH = r"build\IronRook.exe"
OUTPUT_DIR = "Games"
GAMES_COUNT = 100

def main():
    if not os.path.exists(ENGINE_PATH):
        print(f"Error: Engine not found at {ENGINE_PATH}")
        print("Please build the engine first using 'mingw32-make' in 'build' directory.")
        return

    if not os.path.exists(OUTPUT_DIR):
        os.makedirs(OUTPUT_DIR)
        print(f"Created directory: {OUTPUT_DIR}")

    timestamp = time.strftime("%Y%m%d_%H%M%S")
    output_filename = f"selfplay_{timestamp}.pgn"
    output_path = os.path.join(OUTPUT_DIR, output_filename)

    # We reuse the robust logic in tools/selfplay.py by calling it as a subprocess
    # This avoids code duplication and issues with importing checks specific to __main__
    
    cmd = [
        sys.executable, 
        "tools/selfplay.py", 
        ENGINE_PATH, 
        output_path, 
        str(GAMES_COUNT)
    ]

    print(f"Starting generation of {GAMES_COUNT} games...")
    print(f"Output: {output_path}")
    
    try:
        subprocess.run(cmd, check=True)
        print(f"\nDone! Games saved to {output_path}")
    except subprocess.CalledProcessError as e:
        print(f"Error running selfplay: {e}")
    except KeyboardInterrupt:
        print("\nStopped.")

if __name__ == "__main__":
    main()
