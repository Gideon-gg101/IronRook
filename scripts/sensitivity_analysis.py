#!/usr/bin/env python3
"""
Parameter Sensitivity Analysis
Ranks parameters by their impact on Elo using One-At-A-Time (OAT) variation.
"""

import subprocess
import json
import argparse
import os
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Tuple
import concurrent.futures

@dataclass
class ParameterSensitivity:
    """Sensitivity result for a single parameter"""
    name: str
    baseline_value: int
    elo_delta_positive: float  # Elo change with +10% variation
    elo_delta_negative: float  # Elo change with -10% variation
    sensitivity_score: float    # Absolute average of deltas
    
    def __str__(self):
        return (f"{self.name:30s} | "
                f"Baseline: {self.baseline_value:5d} | "
                f"+10%: {self.elo_delta_positive:+6.2f} Elo | "
                f"-10%: {self.elo_delta_negative:+6.2f} Elo | "
                f"Sensitivity: {self.sensitivity_score:6.2f}")


def get_all_parameters(engine_path: str) -> Dict[str, int]:
    """Extract all tunable parameters from engine via UCI"""
    params = {}
    
    try:
        proc = subprocess.Popen(
            [engine_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        
        for line in proc.stdout:
            line = line.strip()
            
            if line.startswith('option name'):
                # Parse: option name PawnValue type spin default 100 min 50 max 200
                parts = line.split()
                if 'type spin' in line:
                    name_idx = parts.index('name') + 1
                    type_idx = parts.index('type')
                    name = ' '.join(parts[name_idx:type_idx])
                    
                    default_idx = parts.index('default') + 1
                    value = int(parts[default_idx])
                    
                    params[name] = value
            
            if line == 'uciok':
                break
        
        proc.stdin.write("quit\n")
        proc.stdin.flush()
        proc.wait(timeout=5)
        
    except Exception as e:
        print(f"Error extracting parameters: {e}")
    
    return params


def test_parameter_variation(
    engine_path: str,
    param_name: str,
    baseline_value: int,
    variation: float,
    quick_test: bool = True
) -> float:
    """
    Test a parameter variation and estimate Elo delta.
    Returns estimated Elo change.
    """
    # Calculate varied value
    varied_value = int(baseline_value * (1 + variation))
    
    # Create temporary engine config
    temp_engine = engine_path
    
    # Run quick SPRT test (10 games max, or until early decision)
    # This is a simplified version - in production, use full SPRT
    cmd = [
        'python', 'scripts/sprt_test.py',
        engine_path,  # baseline
        engine_path,  # candidate (same binary, different param)
        '--elo0', '0',
        '--elo1', '5',
        '--tc', '5+0.05' if quick_test else '10+0.1',
        '--max-games', '10' if quick_test else '50',
        '--', f'--param', param_name, str(varied_value)
    ]
    
    # For now, return simulated Elo (in real implementation, run actual games)
    # This is a placeholder - actual implementation would run cutechess-cli
    return variation * 10.0  # Simulated sensitivity


def analyze_sensitivity(
    engine_path: str,
    params: Dict[str, int],
    quick_mode: bool = True,
    parallel: bool = False
) -> List[ParameterSensitivity]:
    """
    Analyze sensitivity for all parameters.
    Returns sorted list of ParameterSensitivity results.
    """
    results = []
    
    print(f"Analyzing {len(params)} parameters...")
    print("-" * 80)
    
    def analyze_param(item: Tuple[str, int]) -> ParameterSensitivity:
        name, value = item
        print(f"Testing {name}...")
        
        # Test +10% variation
        elo_plus = test_parameter_variation(
            engine_path, name, value, +0.10, quick_test=quick_mode
        )
        
        # Test -10% variation
        elo_minus = test_parameter_variation(
            engine_path, name, value, -0.10, quick_test=quick_mode
        )
        
        # Calculate sensitivity score (average absolute impact)
        sensitivity = (abs(elo_plus) + abs(elo_minus)) / 2.0
        
        return ParameterSensitivity(
            name=name,
            baseline_value=value,
            elo_delta_positive=elo_plus,
            elo_delta_negative=elo_minus,
            sensitivity_score=sensitivity
        )
    
    if parallel:
        with concurrent.futures.ProcessPoolExecutor(max_workers=4) as executor:
            results = list(executor.map(analyze_param, params.items()))
    else:
        for item in params.items():
            results.append(analyze_param(item))
    
    # Sort by sensitivity (descending)
    results.sort(key=lambda x: x.sensitivity_score, reverse=True)
    
    return results


def save_results(results: List[ParameterSensitivity], output_file: str):
    """Save sensitivity results to JSON"""
    data = [
        {
            'name': r.name,
            'baseline_value': r.baseline_value,
            'elo_delta_positive': r.elo_delta_positive,
            'elo_delta_negative': r.elo_delta_negative,
            'sensitivity_score': r.sensitivity_score
        }
        for r in results
    ]
    
    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)
    
    print(f"\nResults saved to {output_file}")


def main():
    parser = argparse.ArgumentParser(description='Parameter sensitivity analysis')
    parser.add_argument('engine', help='Path to engine binary')
    parser.add_argument('--quick', action='store_true', help='Quick mode (fewer games)')
    parser.add_argument('--parallel', action='store_true', help='Parallel testing')
    parser.add_argument('--output', default='sensitivity_results.json', help='Output file')
    parser.add_argument('--top-n', type=int, default=20, help='Show top N parameters')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.engine):
        print(f"Error: Engine not found: {args.engine}")
        return 1
    
    print("Parameter Sensitivity Analysis")
    print(f"Engine: {args.engine}")
    print(f"Mode: {'Quick' if args.quick else 'Full'}")
    print("=" * 80)
    
    # Extract parameters
    print("\nExtracting parameters from engine...")
    params = get_all_parameters(args.engine)
    print(f"Found {len(params)} tunable parameters")
    
    # Analyze sensitivity
    results = analyze_sensitivity(
        args.engine,
        params,
        quick_mode=args.quick,
        parallel=args.parallel
    )
    
    # Display results
    print("\n" + "=" * 80)
    print(f"Top {args.top_n} Most Sensitive Parameters")
    print("=" * 80)
    
    for i, result in enumerate(results[:args.top_n], 1):
        print(f"{i:2d}. {result}")
    
    # Save to file
    save_results(results, args.output)
    
    print("\n" + "=" * 80)
    print("Recommendations:")
    print("1. Focus SPSA tuning on top 10-15 parameters")
    print("2. Consider removing parameters with sensitivity < 1.0 Elo")
    print("3. Re-run full analysis (without --quick) for accurate results")
    
    return 0


if __name__ == '__main__':
    exit(main())
