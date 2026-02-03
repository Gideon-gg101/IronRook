#!/usr/bin/env python3
"""
Simple self-play game generator for tuning data
Generates games with node-based time control
"""

import argparse
import subprocess
import sys
from pathlib import Path
from datetime import datetime

def generate_games(engine_path, output_pgn, total_games, nodes_per_move, concurrency):
    """Generate self-play games using cutechess-cli"""
    
    print(f"Starting game generation...")
    print(f"Target: {total_games:,} games")
    print(f"Nodes/move: {nodes_per_move:,}")
    print(f"Concurrency: {concurrency}")
    print(f"Output: {output_pgn}")
    print()
    
    # Ensure output directory exists
    Path(output_pgn).parent.mkdir(parents=True, exist_ok=True)
    
    # cutechess-cli command
    cmd = [
        "cutechess-cli",
        "-engine", f"cmd={engine_path}", "name=IronRook",
        "-engine", f"cmd={engine_path}", "name=IronRook",
        "-each", f"nodes={nodes_per_move}", "proto=uci",
        "-games", str(total_games),
        "-concurrency", str(concurrency),
        "-pgnout", output_pgn,
        "-recover",
        "-repeat",
        "-openings", "file=data/books/main_book.bin", "format=bin", "order=random"
    ]
    
    print("Command:", " ".join(cmd))
    print()
    
    try:
        # Run cutechess-cli
        process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        
        games_completed = 0
        for line in process.stdout:
            print(line, end='')
            
            # Track progress
            if "Finished game" in line:
                games_completed += 1
                if games_completed % 100 == 0:
                    progress = (games_completed / total_games) * 100
                    print(f"\nProgress: {games_completed:,}/{total_games:,} ({progress:.1f}%)\n")
        
        process.wait()
        
        if process.returncode == 0:
            print(f"\n✅ Successfully generated {total_games:,} games!")
            print(f"Saved to: {output_pgn}")
        else:
            print(f"\n❌ Process failed with return code {process.returncode}")
            sys.exit(1)
            
    except FileNotFoundError:
        print("❌ Error: cutechess-cli not found!")
        print("Please install cutechess-cli or add it to PATH")
        sys.exit(1)
    except KeyboardInterrupt:
        print(f"\n⚠️ Interrupted! Games completed: {games_completed:,}")
        print(f"Partial results saved to: {output_pgn}")
        sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description="Generate self-play games for tuning")
    parser.add_argument("--engine", required=True, help="Path to engine executable")
    parser.add_argument("--output", required=True, help="Output PGN file")
    parser.add_argument("--games", type=int, default=10000, help="Number of games to generate")
    parser.add_argument("--nodes", type=int, default=3000, help="Nodes per move")
    parser.add_argument("--concurrency", type=int, default=8, help="Parallel games")
    
    args = parser.parse_args()
    
    # Validate engine exists
    if not Path(args.engine).exists():
        print(f"❌ Error: Engine not found: {args.engine}")
        sys.exit(1)
    
    generate_games(
        engine_path=args.engine,
        output_pgn=args.output,
        total_games=args.games,
        nodes_per_move=args.nodes,
        concurrency=args.concurrency
    )

if __name__ == "__main__":
    main()
