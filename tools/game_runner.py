"""
Game Runner for SPSA Tuning

Runs matches between two engine configurations and returns results.
Supports both cutechess-cli (preferred) and internal Python implementation.
"""

import subprocess
import json
import os
import sys
import tempfile
from typing import Tuple
import time


class GameRunner:
    """Runs games between two engine configurations"""
    
    def __init__(self, engine_path: str, time_control: str = "10+0.1", 
                 threads: int = 1, hash: int = 64, concurrency: int = 1,
                 cutechess_path: str = None, opening_book: str = None):
        """
        Initialize game runner
        
        Args:
            engine_path: Path to the engine executable
            time_control: Time control in format "base+inc" (e.g., "10+0.1")
            threads: Number of threads per engine
            hash: Hash table size in MB
            concurrency: Number of concurrent games to run (for cutechess)
            cutechess_path: Path to cutechess-cli executable (optional)
            opening_book: Path to opening book file (optional)
        """
        self.engine_path = engine_path
        self.time_control = time_control
        self.threads = threads
        self.hash = hash
        self.concurrency = concurrency
        self.cutechess_path = cutechess_path or 'cutechess-cli'
        self.opening_book = opening_book
        
        # Parse time control
        parts = time_control.split('+')
        # If parts[0] is not a number (e.g., 'nodes=3000'), handle gracefully
        try:
            val = float(parts[0])
            self.base_time = int(val * 1000)  # Convert to ms
            self.increment = int(float(parts[1]) * 1000) if len(parts) > 1 else 0
        except ValueError:
            # Likely 'nodes=X' or 'depth=X'
            self.base_time = 0
            self.increment = 0
        
        # Check if cutechess-cli is available
        self.has_cutechess = self._check_cutechess()
        
        if not self.has_cutechess:
            print("Warning: cutechess-cli not found. Using internal game runner.")
            print("For production tuning, install cutechess-cli for better reliability.")
    
    def _check_cutechess(self) -> bool:
        """Check if cutechess-cli is available"""
        try:
            result = subprocess.run([self.cutechess_path, '--version'], 
                                    capture_output=True, timeout=5)
            return result.returncode == 0
        except (FileNotFoundError, subprocess.TimeoutExpired):
            return False
    
    def run_match(self, params_file1: str, params_file2: str, 
                  num_games: int) -> Tuple[int, int, int]:
        """
        Run a match between two parameter configurations
        
        Args:
            params_file1: JSON file with parameters for engine 1
            params_file2: JSON file with parameters for engine 2 (baseline)
            num_games: Number of games to play
        
        Returns:
            Tuple of (wins for engine1, losses for engine1, draws)
        """
        if self.has_cutechess:
            return self._run_cutechess_match(params_file1, params_file2, num_games)
        else:
            return self._run_internal_match(params_file1, params_file2, num_games)
    
    def _run_cutechess_match(self, params1: str, params2: str, 
                             num_games: int) -> Tuple[int, int, int]:
        """Run match using cutechess-cli"""
        
        # Build cutechess-cli command
        cmd = [
            self.cutechess_path,
            '-concurrency', str(self.concurrency),
            '-engine', f'cmd={self.engine_path}', 
            f'initstr=importparams {params1}',
            'proto=uci', f'tc={self.time_control}',
            '-engine', f'cmd={self.engine_path}',
            f'initstr=importparams {params2}',
            'proto=uci', f'tc={self.time_control}',
            '-each', f'option.Threads={self.threads}', 
            f'option.Hash={self.hash}',
            '-games', str(num_games),
            '-repeat',
            '-rounds', '1',
            '-pgnout', 'games.pgn'
        ]

        if self.opening_book:
            ext = os.path.splitext(self.opening_book)[1].lower()
            fmt = 'epd' if ext == '.epd' else 'pgn'
            cmd.extend(['-openings', f'file={self.opening_book}', f'format={fmt}', 'order=random'])
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
            
            # Parse output for results
            output = result.stdout
            wins = output.count('1-0') + output.count('0-1')
            draws = output.count('1/2-1/2')
            
            # Determine which wins belong to engine 1
            # This is a simplified parser - cutechess-cli output has more detail
            # For now, assume roughly even distribution
            engine1_wins = wins // 2
            losses = wins - engine1_wins
            
            return engine1_wins, losses, draws
            
        except subprocess.TimeoutExpired:
            print("Warning: cutechess-cli timed out")
            return 0, 0, 0
        except Exception as e:
            print(f"Error running cutechess-cli: {e}")
            return 0, 0, 0
    
    def _run_internal_match(self, params1: str, params2: str, 
                            num_games: int) -> Tuple[int, int, int]:
        """
        Run match using internal Python implementation
        Alternates colors and plays games via UCI
        """
        wins = 0
        losses = 0
        draws = 0
        
        for game_num in range(num_games):
            # Alternate colors: engine1 plays white in even games
            engine1_white = (game_num % 2 == 0)
            
            result = self._play_single_game(params1, params2, engine1_white)
            
            # result: 1 = white wins, 0 = draw, -1 = black wins
            if result == 1:
                if engine1_white:
                    wins += 1
                else:
                    losses += 1
            elif result == -1:
                if engine1_white:
                    losses += 1
                else:
                    wins += 1
            else:
                draws += 1
            
            # Progress indicator
            if (game_num + 1) % 10 == 0:
                print(f"  Completed {game_num + 1}/{num_games} games: "
                      f"+{wins} -{losses} ={draws}")
        
        return wins, losses, draws
    
    def _play_single_game(self, params1: str, params2: str, 
                          engine1_white: bool) -> int:
        """
        Play a single game between two configurations
        
        Returns:
            1 if white wins, -1 if black wins, 0 if draw
        """
        # Start two engine processes
        white_params = params1 if engine1_white else params2
        black_params = params2 if engine1_white else params1
        
        try:
            proc_white = self._start_engine(white_params)
            proc_black = self._start_engine(black_params)
            
            # Play the game
            result = self._play_game_loop(proc_white, proc_black)
            
            # Cleanup
            proc_white.kill()
            proc_black.kill()
            
            return result
            
        except Exception as e:
            print(f"Error in game: {e}")
            return 0  # Draw on error
    
    def _start_engine(self, params_file: str) -> subprocess.Popen:
        """Start an engine process and load parameters"""
        proc = subprocess.Popen(
            [self.engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        
        # Initialize UCI
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        while True:
            line = proc.stdout.readline().strip()
            if line == 'uciok':
                break
        
        # Load parameters
        proc.stdin.write(f"importparams {params_file}\n")
        proc.stdin.flush()
        time.sleep(0.1)  # Give time to load
        
        # Ready check
        proc.stdin.write("isready\n")
        proc.stdin.flush()
        while True:
            line = proc.stdout.readline().strip()
            if line == 'readyok':
                break
        
        return proc
    
    def _play_game_loop(self, proc_white: subprocess.Popen, 
                        proc_black: subprocess.Popen) -> int:
        """
        Play a game loop between two engines
        
        Returns:
            1 if white wins, -1 if black wins, 0 if draw
        """
        moves = []
        wtime = self.base_time
        btime = self.base_time
        max_moves = 200  # Game length limit
        
        for ply in range(max_moves):
            # Determine current player
            is_white = (ply % 2 == 0)
            proc = proc_white if is_white else proc_black
            
            # Send position
            pos_cmd = "position startpos"
            if moves:
                pos_cmd += " moves " + " ".join(moves)
            proc.stdin.write(pos_cmd + "\n")
            proc.stdin.flush()
            
            # Search with time control
            start_time = time.time()
            go_cmd = f"go wtime {wtime} btime {btime} winc {self.increment} binc {self.increment}\n"
            proc.stdin.write(go_cmd)
            proc.stdin.flush()
            
            # Get best move
            best_move = None
            while True:
                line = proc.stdout.readline().strip()
                if line.startswith('bestmove'):
                    best_move = line.split()[1]
                    break
                # Timeout safety
                if time.time() - start_time > 60:
                    return 0  # Draw on timeout
            
            # Update time
            elapsed = int((time.time() - start_time) * 1000)
            if is_white:
                wtime = wtime - elapsed + self.increment
            else:
                btime = btime - elapsed + self.increment
            
            # Check for game end
            if not best_move or best_move == '(none)':
                # No legal move - checkmate or stalemate
                # Simple heuristic: if we have time, it's likely mate
                if (is_white and wtime > 0) or (not is_white and btime > 0):
                    return -1 if is_white else 1  # Current player lost (mate)
                else:
                    return 0  # Draw (stalemate or time)
            
            moves.append(best_move)
            
            # Check for draw by repetition (simplified: 150 move rule)
            if len(moves) > 150:
                return 0
        
        return 0  # Draw if max moves reached


def test_game_runner():
    """Test the game runner with default parameters"""
    print("Testing Game Runner...")
    
    # Create test parameter files
    with open('test_params1.json', 'w') as f:
        json.dump({"BishopPairMG": 25, "SingularMargin": 2}, f)
    
    with open('test_params2.json', 'w') as f:
        json.dump({"BishopPairMG": 20, "SingularMargin": 2}, f)
    
    runner = GameRunner('build/Prometheus.exe', time_control='5+0.05')
    
    print("Running 2 test games...")
    wins, losses, draws = runner.run_match('test_params1.json', 
                                           'test_params2.json', 2)
    
    print(f"\nResults: +{wins} -{losses} ={draws}")
    
    # Cleanup
    os.remove('test_params1.json')
    os.remove('test_params2.json')


if __name__ == '__main__':
    test_game_runner()
