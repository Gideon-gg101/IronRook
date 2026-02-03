#!/usr/bin/env python3
"""
Automated Test Suite Runner
Comprehensive regression testing for tactical, endgame, and strategic positions.
"""

import subprocess
import json
import re
import argparse
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple
from enum import Enum

class SuiteType(Enum):
    TACTICAL = "tactical"
    ENDGAME = "endgame"
    FORTRESS = "fortress"
    ZUGZWANG = "zugzwang"
    MATE = "mate"
    STRATEGIC = "strategic"
    PRUNING = "pruning"

@dataclass
class TestPosition:
    """Single test position"""
    fen: str
    description: str
    best_moves: List[str]  # Multiple acceptable moves
    avoid_moves: List[str] = None  # Moves to avoid
    mate_in: Optional[int] = None  # For mate problems
    min_score: Optional[int] = None  # Minimum expected eval
    max_score: Optional[int] = None  # Maximum expected eval
    min_depth: int = 12  # Minimum depth to search

@dataclass
class TestResult:
    """Result of a single test"""
    position_id: str
    fen: str
    description: str
    engine_move: str
    engine_score: int
    depth_reached: int
    passed: bool
    reason: str
    time_ms: int

class TestSuiteRunner:
    def __init__(self, engine_path: str, default_time_ms: int = 5000):
        self.engine_path = engine_path
        self.default_time_ms = default_time_ms
        self.results: List[TestResult] = []
    
    def load_suite(self, suite_path: str) -> List[TestPosition]:
        """Load test suite from EPD file"""
        positions = []
        
        with open(suite_path, 'r') as f:
            for line_num, line in enumerate(f, 1):
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                
                # Parse EPD format: FEN; bm best_move; am avoid_move; id "description";
                pos = self._parse_epd_line(line, line_num)
                if pos:
                    positions.append(pos)
        
        return positions
    
    def _parse_epd_line(self, line: str, line_num: int) -> Optional[TestPosition]:
        """Parse single EPD line"""
        # Split FEN from operations
        parts = line.split(';')
        if not parts:
            return None
        
        fen = parts[0].strip()
        
        # Parse operations
        best_moves = []
        avoid_moves = []
        description = f"Position {line_num}"
        mate_in = None
        min_score = None
        max_score = None
        
        for op in parts[1:]:
            op = op.strip()
            if not op:
                continue
            
            if op.startswith('bm '):
                # Best move(s)
                moves_str = op[3:].strip()
                best_moves = [m.strip() for m in moves_str.split(',')]
            
            elif op.startswith('am '):
                # Avoid move(s)
                moves_str = op[3:].strip()
                avoid_moves = [m.strip() for m in moves_str.split(',')]
            
            elif op.startswith('id '):
                # Description
                description = op[3:].strip().strip('"')
            
            elif op.startswith('dm '):
                # Depth mate
                try:
                    mate_in = int(op[3:].strip())
                except:
                    pass
            
            elif op.startswith('ce '):
                # Centipawn evaluation
                try:
                    score = int(op[3:].strip())
                    min_score = score - 50
                    max_score = score + 50
                except:
                    pass
        
        return TestPosition(
            fen=fen,
            description=description,
            best_moves=best_moves,
            avoid_moves=avoid_moves or [],
            mate_in=mate_in,
            min_score=min_score,
            max_score=max_score
        )
    
    def run_position(self, position: TestPosition) -> TestResult:
        """Run engine on a single position"""
        try:
            proc = subprocess.Popen(
                [self.engine_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Initialize engine
            proc.stdin.write("uci\n")
            proc.stdin.flush()
            
            # Wait for uciok
            for line in proc.stdout:
                if 'uciok' in line:
                    break
            
            # Set position
            proc.stdin.write(f"position fen {position.fen}\n")
            proc.stdin.write(f"go movetime {self.default_time_ms}\n")
            proc.stdin.flush()
            
            # Collect results
            best_move = None
            score = 0
            depth = 0
            time_ms = 0
            
            for line in proc.stdout:
                line = line.strip()
                
                if line.startswith('info'):
                    # Extract depth and score
                    depth_match = re.search(r'depth (\d+)', line)
                    if depth_match:
                        depth = int(depth_match.group(1))
                    
                    score_match = re.search(r'score cp (-?\d+)', line)
                    if score_match:
                        score = int(score_match.group(1))
                    
                    mate_match = re.search(r'score mate (-?\d+)', line)
                    if mate_match:
                        mate = int(mate_match.group(1))
                        score = 30000 if mate > 0 else -30000
                    
                    time_match = re.search(r'time (\d+)', line)
                    if time_match:
                        time_ms = int(time_match.group(1))
                
                elif line.startswith('bestmove'):
                    best_move = line.split()[1]
                    break
            
            proc.stdin.write("quit\n")
            proc.stdin.flush()
            proc.wait(timeout=5)
            
            # Evaluate result
            passed, reason = self._evaluate_result(
                position, best_move, score, depth
            )
            
            return TestResult(
                position_id=position.description,
                fen=position.fen,
                description=position.description,
                engine_move=best_move or "none",
                engine_score=score,
                depth_reached=depth,
                passed=passed,
                reason=reason,
                time_ms=time_ms
            )
        
        except Exception as e:
            return TestResult(
                position_id=position.description,
                fen=position.fen,
                description=position.description,
                engine_move="error",
                engine_score=0,
                depth_reached=0,
                passed=False,
                reason=f"Error: {str(e)}",
                time_ms=0
            )
    
    def _evaluate_result(
        self,
        position: TestPosition,
        engine_move: str,
        score: int,
        depth: int
    ) -> Tuple[bool, str]:
        """Evaluate if result passes the test"""
        # Check depth
        if depth < position.min_depth:
            return False, f"Insufficient depth: {depth} < {position.min_depth}"
        
        # Check best move
        if position.best_moves:
            if engine_move in position.best_moves:
                return True, "Correct move found"
            else:
                return False, f"Wrong move: {engine_move}, expected: {position.best_moves}"
        
        # Check avoid moves
        if position.avoid_moves and engine_move in position.avoid_moves:
            return False, f"Played avoid move: {engine_move}"
        
        # Check mate
        if position.mate_in is not None:
            if abs(score) >= 29000:  # Mate score
                return True, f"Mate found with score {score}"
            else:
                return False, f"Mate not found, score: {score}"
        
        # Check score range
        if position.min_score is not None and score < position.min_score:
            return False, f"Score too low: {score} < {position.min_score}"
        
        if position.max_score is not None and score > position.max_score:
            return False, f"Score too high: {score} > {position.max_score}"
        
        # Default: pass if no failure
        return True, "Position passed"
    
    def run_suite(self, suite_path: str, suite_name: str) -> Dict:
        """Run entire test suite"""
        print(f"\n{'='*80}")
        print(f"Running Test Suite: {suite_name}")
        print(f"Suite File: {suite_path}")
        print(f"{'='*80}\n")
        
        positions = self.load_suite(suite_path)
        print(f"Loaded {len(positions)} positions")
        
        results = []
        passed = 0
        failed = 0
        
        for i, pos in enumerate(positions, 1):
            print(f"\nTest {i}/{len(positions)}: {pos.description}")
            result = self.run_position(pos)
            results.append(result)
            
            if result.passed:
                passed += 1
                print(f"  ✓ PASS - {result.reason}")
            else:
                failed += 1
                print(f"  ✗ FAIL - {result.reason}")
            
            print(f"    Move: {result.engine_move} | Score: {result.engine_score} | "
                  f"Depth: {result.depth_reached} | Time: {result.time_ms}ms")
        
        # Summary
        total = len(positions)
        pass_rate = (passed / total * 100) if total > 0 else 0
        
        print(f"\n{'='*80}")
        print(f"RESULTS: {suite_name}")
        print(f"{'='*80}")
        print(f"Passed: {passed}/{total} ({pass_rate:.1f}%)")
        print(f"Failed: {failed}/{total}")
        
        return {
            'suite_name': suite_name,
            'total': total,
            'passed': passed,
            'failed': failed,
            'pass_rate': pass_rate,
            'results': [asdict(r) for r in results]
        }


def main():
    parser = argparse.ArgumentParser(description='Run automated test suites')
    parser.add_argument('engine', help='Path to engine binary')
    parser.add_argument('--suites', nargs='+', help='Test suite files (EPD)')
    parser.add_argument('--time', type=int, default=5000, help='Time per position (ms)')
    parser.add_argument('--output', default='test_results.json', help='Output file')
    parser.add_argument('--min-pass-rate', type=float, default=80.0, 
                       help='Minimum pass rate to succeed')
    
    args = parser.parse_args()
    
    runner = TestSuiteRunner(args.engine, args.time)
    
    all_results = []
    overall_passed = 0
    overall_total = 0
    
    for suite_path in args.suites:
        suite_name = Path(suite_path).stem
        result = runner.run_suite(suite_path, suite_name)
        all_results.append(result)
        
        overall_passed += result['passed']
        overall_total += result['total']
    
    # Overall summary
    overall_pass_rate = (overall_passed / overall_total * 100) if overall_total > 0 else 0
    
    print(f"\n{'='*80}")
    print(f"OVERALL RESULTS")
    print(f"{'='*80}")
    print(f"Total Positions: {overall_total}")
    print(f"Passed: {overall_passed} ({overall_pass_rate:.1f}%)")
    print(f"Failed: {overall_total - overall_passed}")
    
    # Save results
    with open(args.output, 'w') as f:
        json.dump({
            'overall_pass_rate': overall_pass_rate,
            'suites': all_results
        }, f, indent=2)
    
    print(f"\nResults saved to {args.output}")
    
    # Exit code
    if overall_pass_rate >= args.min_pass_rate:
        print(f"\n✓ SUCCESS: Pass rate {overall_pass_rate:.1f}% >= {args.min_pass_rate}%")
        return 0
    else:
        print(f"\n✗ FAILURE: Pass rate {overall_pass_rate:.1f}% < {args.min_pass_rate}%")
        return 1


if __name__ == '__main__':
    exit(main())
