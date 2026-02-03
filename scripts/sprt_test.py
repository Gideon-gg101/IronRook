#!/usr/bin/env python3
"""
SPRT (Sequential Probability Ratio Test) Regression Testing
Runs games between baseline and candidate engines until statistical confidence is reached.
"""

import subprocess
import json
import math
import argparse
import os
from pathlib import Path
from dataclasses import dataclass
from typing import Tuple, Optional

@dataclass
class SPRTConfig:
    """SPRT test configuration"""
    elo0: float = 0.0      # H0: Elo <= elo0 (null hypothesis)
    elo1: float = 5.0      # H1: Elo >= elo1 (alternative hypothesis)
    alpha: float = 0.05    # Type I error (false positive)
    beta: float = 0.05     # Type II error (false negative)
    draw_elo: float = 200.0  # Elo for 50% draw rate

class SPRTTest:
    def __init__(self, config: SPRTConfig):
        self.config = config
        self.wins = 0
        self.losses = 0
        self.draws = 0
        self.llr = 0.0  # Log Likelihood Ratio
        
        # Calculate SPRT bounds
        self.lower_bound = math.log(config.beta / (1 - config.alpha))
        self.upper_bound = math.log((1 - config.beta) / config.alpha)
        
    def elo_to_prob(self, elo: float) -> float:
        """Convert Elo difference to win probability"""
        return 1.0 / (1.0 + 10.0 ** (-elo / 400.0))
    
    def calculate_llr(self, wins: int, losses: int, draws: int) -> float:
        """Calculate Log Likelihood Ratio"""
        # Win/loss/draw probabilities under H0 and H1
        w0 = self.elo_to_prob(self.config.elo0)
        l0 = self.elo_to_prob(-self.config.elo0)
        d0 = 1.0 - w0 - l0
        
        w1 = self.elo_to_prob(self.config.elo1)
        l1 = self.elo_to_prob(-self.config.elo1)
        d1 = 1.0 - w1 - l1
        
        # Avoid log(0)
        epsilon = 1e-10
        w0, l0, d0 = max(w0, epsilon), max(l0, epsilon), max(d0, epsilon)
        w1, l1, d1 = max(w1, epsilon), max(l1, epsilon), max(d1, epsilon)
        
        llr = wins * math.log(w1 / w0) + losses * math.log(l1 / l0) + draws * math.log(d1 / d0)
        return llr
    
    def add_game(self, result: str) -> Optional[str]:
        """
        Add game result and update LLR.
        Returns 'PASS', 'FAIL', or None if test continues.
        """
        if result == '1-0':
            self.wins += 1
        elif result == '0-1':
            self.losses += 1
        elif result == '1/2-1/2':
            self.draws += 1
        else:
            raise ValueError(f"Invalid result: {result}")
        
        self.llr = self.calculate_llr(self.wins, self.losses, self.draws)
        
        if self.llr >= self.upper_bound:
            return 'PASS'
        elif self.llr <= self.lower_bound:
            return 'FAIL'
        return None
    
    def status(self) -> dict:
        """Return current test status"""
        total = self.wins + self.losses + self.draws
        elo = self.estimate_elo() if total > 0 else 0.0
        
        return {
            'games': total,
            'wins': self.wins,
            'losses': self.losses,
            'draws': self.draws,
            'llr': self.llr,
            'lower_bound': self.lower_bound,
            'upper_bound': self.upper_bound,
            'elo': elo,
            'progress': self._progress_percentage()
        }
    
    def estimate_elo(self) -> float:
        """Estimate current Elo difference"""
        total = self.wins + self.losses + self.draws
        if total == 0:
            return 0.0
        
        score = (self.wins + 0.5 * self.draws) / total
        
        # Avoid edge cases
        score = max(0.001, min(0.999, score))
        
        return -400.0 * math.log10(1.0 / score - 1.0)
    
    def _progress_percentage(self) -> float:
        """Estimate progress percentage (heuristic)"""
        if self.llr >= self.upper_bound or self.llr <= self.lower_bound:
            return 100.0
        
        # Normalize LLR to [0, 1] range
        range_size = self.upper_bound - self.lower_bound
        normalized = (self.llr - self.lower_bound) / range_size
        return normalized * 100.0


def run_cutechess_game(engine1: str, engine2: str, time_control: str, opening_book: Optional[str] = None) -> str:
    """
    Run a single game using cutechess-cli.
    Returns result from engine1's perspective: '1-0', '0-1', or '1/2-1/2'
    """
    cmd = [
        'cutechess-cli',
        '-engine', f'cmd={engine1}', f'name=Baseline',
        '-engine', f'cmd={engine2}', f'name=Candidate',
        '-each', f'tc={time_control}', 'proto=uci',
        '-games', '1',
        '-repeat', '2',  # Play both sides
        '-pgnout', 'sprt_games.pgn',
        '-openings', f'file={opening_book}', 'format=pgn', 'order=random'
    ]
    
    if not opening_book or not os.path.exists(opening_book):
        cmd = [c for c in cmd if not c.startswith('-openings') and 'file=' not in c and 'format=' not in c and 'order=' not in c]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    # Parse result from output
    # cutechess-cli outputs scores like "Score of Baseline vs Candidate: 1 - 0 - 0"
    for line in result.stdout.split('\n'):
        if 'Score of Baseline vs Candidate:' in line:
            parts = line.split(':')[1].strip().split('-')
            wins = int(parts[0].strip())
            losses = int(parts[1].strip())
            draws = int(parts[2].strip())
            
            if wins > losses:
                return '1-0'
            elif losses > wins:
                return '0-1'
            else:
                return '1/2-1/2'
    
    # Fallback: assume draw if parsing failed
    return '1/2-1/2'


def main():
    parser = argparse.ArgumentParser(description='Run SPRT regression test')
    parser.add_argument('baseline', help='Path to baseline engine')
    parser.add_argument('candidate', help='Path to candidate engine')
    parser.add_argument('--elo0', type=float, default=0.0, help='H0 Elo threshold')
    parser.add_argument('--elo1', type=float, default=5.0, help='H1 Elo threshold')
    parser.add_argument('--alpha', type=float, default=0.05, help='Type I error rate')
    parser.add_argument('--beta', type=float, default=0.05, help='Type II error rate')
    parser.add_argument('--tc', default='10+0.1', help='Time control (e.g., 10+0.1)')
    parser.add_argument('--book', help='Opening book (PGN)')
    parser.add_argument('--max-games', type=int, default=10000, help='Max games before timeout')
    
    args = parser.parse_args()
    
    # Validate engines exist
    if not os.path.exists(args.baseline):
        print(f"Error: Baseline engine not found: {args.baseline}")
        return 1
    if not os.path.exists(args.candidate):
        print(f"Error: Candidate engine not found: {args.candidate}")
        return 1
    
    config = SPRTConfig(
        elo0=args.elo0,
        elo1=args.elo1,
        alpha=args.alpha,
        beta=args.beta
    )
    
    sprt = SPRTTest(config)
    
    print(f"SPRT Regression Test")
    print(f"H0: Elo <= {config.elo0} | H1: Elo >= {config.elo1}")
    print(f"Alpha: {config.alpha} | Beta: {config.beta}")
    print(f"LLR bounds: [{sprt.lower_bound:.3f}, {sprt.upper_bound:.3f}]")
    print(f"Baseline: {args.baseline}")
    print(f"Candidate: {args.candidate}")
    print(f"Time Control: {args.tc}")
    print("-" * 60)
    
    game_num = 0
    while game_num < args.max_games:
        game_num += 1
        
        # Run game
        result = run_cutechess_game(args.baseline, args.candidate, args.tc, args.book)
        
        # Update SPRT
        decision = sprt.add_game(result)
        
        # Print status
        status = sprt.status()
        print(f"Game {game_num}: {result} | "
              f"W-L-D: {status['wins']}-{status['losses']}-{status['draws']} | "
              f"LLR: {status['llr']:.3f} | "
              f"Elo: {status['elo']:+.1f} | "
              f"Progress: {status['progress']:.1f}%")
        
        if decision:
            print("-" * 60)
            print(f"SPRT Decision: {decision}")
            print(f"Final Stats: {status['games']} games, "
                  f"Elo: {status['elo']:+.1f} ± {self._elo_error_margin(status):.1f}")
            return 0 if decision == 'PASS' else 1
    
    print("-" * 60)
    print(f"TIMEOUT: Reached max games ({args.max_games})")
    print(f"Final LLR: {sprt.llr:.3f} (inconclusive)")
    return 2

def _elo_error_margin(status: dict) -> float:
    """Calculate Elo error margin (95% confidence)"""
    total = status['games']
    if total < 10:
        return 999.9
    
    # Simplified error margin calculation
    return 400.0 / math.sqrt(total)


if __name__ == '__main__':
    exit(main())
