#!/usr/bin/env python3
"""
Continuous Self-Play System
Automated regression testing with 1000+ games, SPRT, and auto-rollback.
"""

import subprocess
import json
import os
import shutil
import time
import argparse
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
import sys

# Import SPRT from our existing script
sys.path.append(str(Path(__file__).parent))
from sprt_test import SPRTTest, SPRTConfig

@dataclass
class SelfPlayConfig:
    """Self-play system configuration"""
    baseline_binary: str
    candidate_binary: str
    games_per_round: int = 1000
    time_controls: List[str] = None  # e.g., ['10+0.1', '5+0.05', '60+0.6']
    opening_books: List[str] = None
    sprt_elo0: float = -5.0  # Reject if Elo < -5
    sprt_elo1: float = 0.0   # Accept if Elo >= 0
    sprt_alpha: float = 0.05
    sprt_beta: float = 0.05
    auto_rollback: bool = True
    distributed: bool = False
    worker_count: int = 1
    output_dir: str = 'selfplay_results'

@dataclass
class TestResult:
    """Result of a self-play test"""
    timestamp: str
    candidate_version: str
    total_games: int
    wins: int
    losses: int
    draws: int
    elo_estimate: float
    elo_error: float
    sprt_decision: str  # 'PASS', 'FAIL', 'TIMEOUT'
    time_control: str
    opening_book: str

class ContinuousSelfPlay:
    def __init__(self, config: SelfPlayConfig):
        self.config = config
        self.results_history: List[TestResult] = []
        self.current_baseline = config.baseline_binary
        
        # Create output directory
        Path(config.output_dir).mkdir(exist_ok=True)
        
        # Load or initialize history
        self.history_file = Path(config.output_dir) / 'history.json'
        self._load_history()
    
    def _load_history(self):
        """Load testing history from disk"""
        if self.history_file.exists():
            with open(self.history_file, 'r') as f:
                data = json.load(f)
                self.results_history = [
                    TestResult(**r) for r in data.get('results', [])
                ]
                self.current_baseline = data.get('current_baseline', self.config.baseline_binary)
    
    def _save_history(self):
        """Save testing history to disk"""
        with open(self.history_file, 'w') as f:
            json.dump({
                'current_baseline': self.current_baseline,
                'results': [asdict(r) for r in self.results_history]
            }, f, indent=2)
    
    def run_sprt_test(
        self,
        time_control: str,
        opening_book: Optional[str],
        max_games: int
    ) -> Tuple[str, int, int, int, float]:
        """
        Run SPRT test for a single configuration.
        Returns (decision, wins, losses, draws, elo_estimate)
        """
        sprt_config = SPRTConfig(
            elo0=self.config.sprt_elo0,
            elo1=self.config.sprt_elo1,
            alpha=self.config.sprt_alpha,
            beta=self.config.sprt_beta
        )
        
        sprt = SPRTTest(sprt_config)
        
        games_played = 0
        pgn_file = Path(self.config.output_dir) / f'games_{time_control.replace("+", "_")}.pgn'
        
        while games_played < max_games:
            # Run batch of games
            batch_size = min(100, max_games - games_played)
            
            cmd = [
                'cutechess-cli',
                '-engine', f'cmd={self.current_baseline}', 'name=Baseline',
                '-engine', f'cmd={self.config.candidate_binary}', 'name=Candidate',
                '-each', f'tc={time_control}', 'proto=uci',
                '-games', str(batch_size),
                '-repeat', '2',  # Play both sides
                '-pgnout', str(pgn_file),
                '-recover'
            ]
            
            if opening_book and os.path.exists(opening_book):
                cmd.extend([
                    '-openings', f'file={opening_book}', 'format=pgn', 'order=random'
                ])
            
            print(f"  Running games {games_played + 1}-{games_played + batch_size}...")
            result = subprocess.run(cmd, capture_output=True, text=True)
            
            # Parse results
            for line in result.stdout.split('\n'):
                if 'Score of Baseline vs Candidate:' in line:
                    parts = line.split(':')[1].strip().split('-')
                    w = int(parts[0].strip())
                    l = int(parts[1].strip())
                    d = int(parts[2].strip())
                    
                    # Add games to SPRT
                    for _ in range(w):
                        decision = sprt.add_game('1-0')
                        if decision:
                            return decision, sprt.wins, sprt.losses, sprt.draws, sprt.estimate_elo()
                    
                    for _ in range(l):
                        decision = sprt.add_game('0-1')
                        if decision:
                            return decision, sprt.wins, sprt.losses, sprt.draws, sprt.estimate_elo()
                    
                    for _ in range(d):
                        decision = sprt.add_game('1/2-1/2')
                        if decision:
                            return decision, sprt.wins, sprt.losses, sprt.draws, sprt.estimate_elo()
            
            games_played += batch_size
            
            # Print progress
            status = sprt.status()
            print(f"    Progress: {status['games']} games | "
                  f"W-L-D: {status['wins']}-{status['losses']}-{status['draws']} | "
                  f"LLR: {status['llr']:.2f} [{sprt.lower_bound:.2f}, {sprt.upper_bound:.2f}] | "
                  f"Elo: {status['elo']:+.1f}")
        
        # Timeout
        return 'TIMEOUT', sprt.wins, sprt.losses, sprt.draws, sprt.estimate_elo()
    
    def run_comprehensive_test(self) -> bool:
        """
        Run comprehensive test across all time controls and opening books.
        Returns True if candidate passes all tests.
        """
        print(f"\n{'='*80}")
        print(f"COMPREHENSIVE REGRESSION TEST")
        print(f"Baseline: {self.current_baseline}")
        print(f"Candidate: {self.config.candidate_binary}")
        print(f"{'='*80}\n")
        
        # Use default values if not provided
        time_controls = self.config.time_controls or ['10+0.1']
        opening_books = self.config.opening_books or [None]
        
        all_passed = True
        
        for tc in time_controls:
            for book in opening_books:
                book_name = Path(book).name if book else 'startpos'
                
                print(f"\nTesting TC={tc}, Book={book_name}")
                print(f"{'-'*80}")
                
                decision, wins, losses, draws, elo = self.run_sprt_test(
                    tc, book, self.config.games_per_round
                )
                
                total = wins + losses + draws
                elo_error = 400.0 / max(1, total ** 0.5)
                
                result = TestResult(
                    timestamp=datetime.now().isoformat(),
                    candidate_version=self.config.candidate_binary,
                    total_games=total,
                    wins=wins,
                    losses=losses,
                    draws=draws,
                    elo_estimate=elo,
                    elo_error=elo_error,
                    sprt_decision=decision,
                    time_control=tc,
                    opening_book=book_name
                )
                
                self.results_history.append(result)
                
                print(f"\nRESULT: {decision}")
                print(f"Games: {total} | W-L-D: {wins}-{losses}-{draws}")
                print(f"Elo: {elo:+.1f} \u00b1 {elo_error:.1f}")
                
                if decision == 'FAIL':
                    all_passed = False
                    print(f"⚠️  REGRESSION DETECTED in TC={tc}, Book={book_name}")
                    if self.config.auto_rollback:
                        print(f"⚠️  AUTO-ROLLBACK ENABLED - Candidate REJECTED")
                        break
                elif decision == 'PASS':
                    print(f"✓ PASSED in TC={tc}, Book={book_name}")
        
            if not all_passed and self.config.auto_rollback:
                break
        
        self._save_history()
        
        return all_passed
    
    def promote_candidate(self):
        """Promote candidate to new baseline"""
        print(f"\n{'='*80}")
        print(f"PROMOTING CANDIDATE TO BASELINE")
        print(f"{'='*80}")
        
        # Backup old baseline
        backup_dir = Path(self.config.output_dir) / 'baseline_archive'
        backup_dir.mkdir(exist_ok=True)
        
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        backup_path = backup_dir / f'baseline_{timestamp}.exe'
        
        shutil.copy(self.current_baseline, backup_path)
        print(f"Backed up baseline to: {backup_path}")
        
        # Promote candidate
        shutil.copy(self.config.candidate_binary, self.current_baseline)
        print(f"Promoted candidate to: {self.current_baseline}")
        
        self._save_history()
    
    def run(self):
        """Main self-play loop"""
        passed = self.run_comprehensive_test()
        
        if passed:
            print(f"\n{'='*80}")
            print(f"✓ ALL TESTS PASSED")
            print(f"{'='*80}")
            self.promote_candidate()
            return 0
        else:
            print(f"\n{'='*80}")
            print(f"✗ TESTS FAILED - CANDIDATE REJECTED")
            print(f"{'='*80}")
            if self.config.auto_rollback:
                print(f"Baseline preserved (no rollback needed)")
            return 1


def main():
    parser = argparse.ArgumentParser(description='Continuous self-play regression testing')
    parser.add_argument('--baseline', required=True, help='Baseline engine binary')
    parser.add_argument('--candidate', required=True, help='Candidate engine binary')
    parser.add_argument('--games', type=int, default=1000, help='Games per round')
    parser.add_argument('--tc', nargs='+', default=['10+0.1'], help='Time controls')
    parser.add_argument('--books', nargs='+', help='Opening books (PGN)')
    parser.add_argument('--elo0', type=float, default=-5.0, help='SPRT H0 threshold')
    parser.add_argument('--elo1', type=float, default=0.0, help='SPRT H1 threshold')
    parser.add_argument('--no-rollback', action='store_true', help='Disable auto-rollback')
    parser.add_argument('--output', default='selfplay_results', help='Output directory')
    
    args = parser.parse_args()
    
    config = SelfPlayConfig(
        baseline_binary=args.baseline,
        candidate_binary=args.candidate,
        games_per_round=args.games,
        time_controls=args.tc,
        opening_books=args.books,
        sprt_elo0=args.elo0,
        sprt_elo1=args.elo1,
        auto_rollback=not args.no_rollback,
        output_dir=args.output
    )
    
    system = ContinuousSelfPlay(config)
    return system.run()


if __name__ == '__main__':
    exit(main())
