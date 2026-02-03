#!/usr/bin/env python3
"""
Perft (Performance Test) for move generation validation
Verifies correctness of move generation by counting leaf nodes.
"""

import subprocess
import json
import argparse
from typing import Dict, List, Tuple
from dataclasses import dataclass, asdict

@dataclass
class PerftPosition:
    """Perft test position with expected node counts"""
    fen: str
    description: str
    expected_nodes: Dict[int, int]  # depth -> node_count

# Standard perft positions with known solutions
PERFT_POSITIONS = [
    PerftPosition(
        fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        description="Starting position",
        expected_nodes={
            1: 20,
            2: 400,
            3: 8902,
            4: 197281,
            5: 4865609,
            6: 119060324
        }
    ),
    PerftPosition(
        fen="r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        description="Kiwipete position",
        expected_nodes={
            1: 48,
            2: 2039,
            3: 97862,
            4: 4085603,
            5: 193690690
        }
    ),
    PerftPosition(
        fen="8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        description="Position 3",
        expected_nodes={
            1: 14,
            2: 191,
            3: 2812,
            4: 43238,
            5: 674624,
            6: 11030083
        }
    ),
    PerftPosition(
        fen="r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        description="Position 4",
        expected_nodes={
            1: 6,
            2: 264,
            3: 9467,
            4: 422333,
            5: 15833292
        }
    ),
    PerftPosition(
        fen="rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        description="Position 5",
        expected_nodes={
            1: 44,
            2: 1486,
            3: 62379,
            4: 2103487,
            5: 89941194
        }
    ),
    PerftPosition(
        fen="r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        description="Position 6",
        expected_nodes={
            1: 46,
            2: 2079,
            3: 89890,
            4: 3894594,
            5: 164075551
        }
    )
]

def run_perft_uci(engine_path: str, fen: str, depth: int) -> int:
    """
    Run perft via UCI 'go perft' command.
    Returns node count.
    """
    try:
        proc = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Initialize
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        
        for line in proc.stdout:
            if 'uciok' in line:
                break
        
        # Run perft
        proc.stdin.write(f"position fen {fen}\n")
        proc.stdin.write(f"go perft {depth}\n")
        proc.stdin.flush()
        
        nodes = 0
        for line in proc.stdout:
            line = line.strip()
            
            # Look for total node count
            if line.startswith('Nodes searched:'):
                nodes = int(line.split(':')[1].strip())
                break
            elif ': ' in line and line.split(':')[0].strip().isdigit():
                # Some engines use "Depth X: NODES" format
                parts = line.split(':')
                if len(parts) == 2:
                    try:
                        nodes = int(parts[1].strip())
                    except:
                        pass
        
        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=60)
        
        return nodes
    
    except Exception as e:
        print(f"Error running perft: {e}")
        return -1

def run_perft_test(engine_path: str, max_depth: int = 5) -> Dict:
    """Run all perft tests"""
    results = []
    total_passed = 0
    total_failed = 0
    
    print(f"\n{'='*80}")
    print(f"PERFT (Performance Test) - Move Generation Validation")
    print(f"Engine: {engine_path}")
    print(f"Max Depth: {max_depth}")
    print(f"{'='*80}\n")
    
    for pos_idx, position in enumerate(PERFT_POSITIONS, 1):
        print(f"\nPosition {pos_idx}: {position.description}")
        print(f"FEN: {position.fen}")
        print(f"{'-'*80}")
        
        pos_results = {
            'description': position.description,
            'fen': position.fen,
            'tests': []
        }
        
        for depth in sorted(position.expected_nodes.keys()):
            if depth > max_depth:
                continue
            
            expected = position.expected_nodes[depth]
            
            print(f"  Depth {depth}... ", end='', flush=True)
            actual = run_perft_uci(engine_path, position.fen, depth)
            
            passed = actual == expected
            if passed:
                total_passed += 1
                print(f"✓ PASS (nodes: {actual:,})")
            else:
                total_failed += 1
                print(f"✗ FAIL (expected: {expected:,}, got: {actual:,})")
            
            pos_results['tests'].append({
                'depth': depth,
                'expected_nodes': expected,
                'actual_nodes': actual,
                'passed': passed
            })
        
        results.append(pos_results)
    
    # Summary
    total_tests = total_passed + total_failed
    pass_rate = (total_passed / total_tests * 100) if total_tests > 0 else 0
    
    print(f"\n{'='*80}")
    print(f"PERFT RESULTS")
    print(f"{'='*80}")
    print(f"Total Tests: {total_tests}")
    print(f"Passed: {total_passed} ({pass_rate:.1f}%)")
    print(f"Failed: {total_failed}")
    
    return {
        'total_tests': total_tests,
        'passed': total_passed,
        'failed': total_failed,
        'pass_rate': pass_rate,
        'positions': results
    }

def main():
    parser = argparse.ArgumentParser(description='Run perft tests')
    parser.add_argument('engine', help='Path to engine binary')
    parser.add_argument('--max-depth', type=int, default=5, help='Maximum depth to test')
    parser.add_argument('--output', default='perft_results.json', help='Output file')
    
    args = parser.parse_args()
    
    results = run_perft_test(args.engine, args.max_depth)
    
    # Save results
    with open(args.output, 'w') as f:
        json.dump(results, f, indent=2)
    
    print(f"\nResults saved to {args.output}")
    
    # Exit code
    if results['failed'] == 0:
        print(f"\n✓ SUCCESS: All perft tests passed!")
        return 0
    else:
        print(f"\n✗ FAILURE: {results['failed']} perft test(s) failed!")
        return 1

if __name__ == '__main__':
    exit(main())
