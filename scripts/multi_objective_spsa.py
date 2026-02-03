#!/usr/bin/env python3
"""
Multi-Objective SPSA Tuning
Optimize for Elo, Nodes, and Time simultaneously using weighted objectives.
"""

import subprocess
import json
import math
import random
import argparse
import os
from pathlib import Path
from dataclasses import dataclass
from typing import Dict, List, Tuple, Optional

@dataclass
class MultiObjectiveConfig:
    """Multi-objective optimization configuration"""
    weight_elo: float = 1.0      # Weight for Elo (playing strength)
    weight_nodes: float = 0.1    # Weight for node efficiency (lower is better)
    weight_time: float = 0.1     # Weight for time efficiency (lower is better)
    normalize_elo: float = 50.0  # Normalization constant for Elo
    normalize_nodes: float = 1e6 # Normalization constant for nodes
    normalize_time: float = 10.0 # Normalization constant for time (seconds)

@dataclass
class GameMetrics:
    """Metrics from a single game"""
    result: str           # '1-0', '0-1', '1/2-1/2'
    nodes: int           # Total nodes searched
    time_ms: int         # Total time in milliseconds
    nps: float           # Nodes per second

class MultiObjectiveSPSA:
    def __init__(self, config: MultiObjectiveConfig, params: Dict[str, dict]):
        self.config = config
        self.params = params
        self.iteration = 0
        self.best_fitness = -float('inf')
        self.best_params = {name: p['default'] for name, p in params.items()}
        
    def calculate_fitness(self, elo: float, avg_nodes: float, avg_time: float) -> float:
        """
        Calculate multi-objective fitness score.
        fitness = w1*Elo - w2*log(Nodes) - w3*log(Time)
        
        Higher Elo is better, lower Nodes/Time is better.
        """
        # Normalize metrics
        norm_elo = elo / self.config.normalize_elo
        norm_nodes = math.log(max(1.0, avg_nodes / self.config.normalize_nodes))
        norm_time = math.log(max(0.001, avg_time / self.config.normalize_time))
        
        fitness = (
            self.config.weight_elo * norm_elo -
            self.config.weight_nodes * norm_nodes -
            self.config.weight_time * norm_time
        )
        
        return fitness
    
    def perturb_params(self, c: float) -> Tuple[Dict[str, int], Dict[str, int]]:
        """
        Generate two parameter sets (plus and minus perturbations).
        Returns (params_plus, params_minus)
        """
        delta = {}
        for name in self.params:
            # Binary perturbation: +1 or -1
            delta[name] = random.choice([-1, 1])
        
        params_plus = {}
        params_minus = {}
        
        for name, info in self.params.items():
            current = self.best_params[name]
            step = int(c * info['step'])
            
            params_plus[name] = self._clamp(current + delta[name] * step, info)
            params_minus[name] = self._clamp(current - delta[name] * step, info)
        
        return params_plus, params_minus, delta
    
    def _clamp(self, value: int, info: dict) -> int:
        """Clamp parameter value to valid range"""
        return max(info['min'], min(info['max'], int(value)))
    
    def update_params(self, delta: Dict[str, int], fitness_plus: float, 
                     fitness_minus: float, a: float) -> Dict[str, int]:
        """
        Update parameters based on fitness gradient.
        """
        gradient_estimate = (fitness_plus - fitness_minus) / 2.0
        
        new_params = {}
        for name, info in self.params.items():
            current = self.best_params[name]
            # Gradient ascent (maximize fitness)
            update = a * gradient_estimate * delta[name]
            new_params[name] = self._clamp(current + int(update), info)
        
        return new_params


def run_match_with_metrics(
    engine_path: str,
    params: Dict[str, int],
    baseline_path: str,
    games: int = 10,
    tc: str = '10+0.1'
) -> Tuple[float, float, float]:
    """
    Run a match and collect metrics.
    Returns (elo_estimate, avg_nodes, avg_time_seconds)
    """
    # Create temp config with parameters
    config = {'uci_options': {name: val for name, val in params.items()}}
    config_file = 'temp_multi_obj_config.json'
    with open(config_file, 'w') as f:
        json.dump(config, f)
    
    # Run cutechess-cli match with PGN output for metric extraction
    pgn_file = 'temp_multi_obj_games.pgn'
    cmd = [
        'cutechess-cli',
        '-engine', f'cmd={engine_path}', 'name=Candidate',
        '-engine', f'cmd={baseline_path}', 'name=Baseline',
        '-each', f'tc={tc}', 'proto=uci',
        '-games', str(games),
        '-repeat', '2',
        '-pgnout', pgn_file,
        '-recover'
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    # Parse Elo from cutechess output
    elo = 0.0
    for line in result.stdout.split('\n'):
        if 'Elo difference:' in line:
            try:
                elo = float(line.split(':')[1].strip().split()[0])
            except:
                pass
    
    # Parse metrics from PGN (simplified - in production, use proper PGN parser)
    total_nodes = 0
    total_time = 0
    game_count = 0
    
    if os.path.exists(pgn_file):
        with open(pgn_file, 'r') as f:
            for line in f:
                # Look for PGN tags with node/time info
                # This is simplified - actual PGN may not have these tags
                if '[Nodes "' in line:
                    try:
                        nodes = int(line.split('"')[1])
                        total_nodes += nodes
                    except:
                        pass
                if '[Time "' in line:
                    try:
                        time = float(line.split('"')[1])
                        total_time += time
                    except:
                        pass
                if '[Result "' in line:
                    game_count += 1
    
    # Fallback values if metrics not available
    avg_nodes = total_nodes / max(1, game_count) if game_count > 0 else 1e6
    avg_time = total_time / max(1, game_count) if game_count > 0 else 10.0
    
    # Cleanup
    if os.path.exists(config_file):
        os.remove(config_file)
    
    return elo, avg_nodes, avg_time


def main():
    parser = argparse.ArgumentParser(description='Multi-objective SPSA tuning')
    parser.add_argument('engine', help='Path to engine binary')
    parser.add_argument('baseline', help='Path to baseline engine')
    parser.add_argument('--params', required=True, help='JSON file with parameter definitions')
    parser.add_argument('--iterations', type=int, default=100, help='Number of SPSA iterations')
    parser.add_argument('--games', type=int, default=20, help='Games per evaluation')
    parser.add_argument('--tc', default='10+0.1', help='Time control')
    parser.add_argument('--weight-elo', type=float, default=1.0, help='Weight for Elo')
    parser.add_argument('--weight-nodes', type=float, default=0.1, help='Weight for node efficiency')
    parser.add_argument('--weight-time', type=float, default=0.1, help='Weight for time efficiency')
    parser.add_argument('--output', default='multi_obj_results.json', help='Output file')
    
    args = parser.parse_args()
    
    # Load parameter definitions
    with open(args.params, 'r') as f:
        params = json.load(f)
    
    config = MultiObjectiveConfig(
        weight_elo=args.weight_elo,
        weight_nodes=args.weight_nodes,
        weight_time=args.weight_time
    )
    
    spsa = MultiObjectiveSPSA(config, params)
    
    print("Multi-Objective SPSA Tuning")
    print(f"Optimizing: Elo (w={config.weight_elo}), "
          f"Nodes (w={config.weight_nodes}), "
          f"Time (w={config.weight_time})")
    print("=" * 80)
    
    history = []
    
    for iteration in range(1, args.iterations + 1):
        # SPSA decay schedules
        a = 100.0 / (iteration + 100)  # Step size
        c = 0.1 / (iteration ** 0.166) # Perturbation size
        
        # Generate perturbations
        params_plus, params_minus, delta = spsa.perturb_params(c)
        
        print(f"\nIteration {iteration}/{args.iterations}")
        print("-" * 80)
        
        # Evaluate plus perturbation
        print("Evaluating (+) perturbation...")
        elo_plus, nodes_plus, time_plus = run_match_with_metrics(
            args.engine, params_plus, args.baseline, args.games, args.tc
        )
        fitness_plus = spsa.calculate_fitness(elo_plus, nodes_plus, time_plus)
        print(f"  Elo: {elo_plus:+.1f} | Nodes: {nodes_plus:.0f} | "
              f"Time: {time_plus:.2f}s | Fitness: {fitness_plus:.3f}")
        
        # Evaluate minus perturbation
        print("Evaluating (-) perturbation...")
        elo_minus, nodes_minus, time_minus = run_match_with_metrics(
            args.engine, params_minus, args.baseline, args.games, args.tc
        )
        fitness_minus = spsa.calculate_fitness(elo_minus, nodes_minus, time_minus)
        print(f"  Elo: {elo_minus:+.1f} | Nodes: {nodes_minus:.0f} | "
              f"Time: {time_minus:.2f}s | Fitness: {fitness_minus:.3f}")
        
        # Update parameters
        new_params = spsa.update_params(delta, fitness_plus, fitness_minus, a)
        
        # Evaluate new parameters
        print("Evaluating updated parameters...")
        elo_new, nodes_new, time_new = run_match_with_metrics(
            args.engine, new_params, args.baseline, args.games, args.tc
        )
        fitness_new = spsa.calculate_fitness(elo_new, nodes_new, time_new)
        
        # Update best if improved
        if fitness_new > spsa.best_fitness:
            spsa.best_fitness = fitness_new
            spsa.best_params = new_params
            print(f"✓ NEW BEST | Fitness: {fitness_new:.3f} | "
                  f"Elo: {elo_new:+.1f} | Nodes: {nodes_new:.0f} | "
                  f"Time: {time_new:.2f}s")
        else:
            print(f"  No improvement | Fitness: {fitness_new:.3f}")
        
        # Log history
        history.append({
            'iteration': iteration,
            'best_fitness': spsa.best_fitness,
            'best_params': spsa.best_params.copy(),
            'current_elo': elo_new,
            'current_nodes': nodes_new,
            'current_time': time_new
        })
        
        # Save checkpoint
        if iteration % 10 == 0:
            with open(args.output, 'w') as f:
                json.dump({
                    'best_params': spsa.best_params,
                    'best_fitness': spsa.best_fitness,
                    'history': history
                }, f, indent=2)
            print(f"Checkpoint saved to {args.output}")
    
    # Final results
    print("\n" + "=" * 80)
    print("OPTIMIZATION COMPLETE")
    print("=" * 80)
    print(f"Best Fitness: {spsa.best_fitness:.3f}")
    print("\nBest Parameters:")
    for name, value in spsa.best_params.items():
        print(f"  {name}: {value}")
    
    # Save final results
    with open(args.output, 'w') as f:
        json.dump({
            'best_params': spsa.best_params,
            'best_fitness': spsa.best_fitness,
            'config': {
                'weight_elo': config.weight_elo,
                'weight_nodes': config.weight_nodes,
                'weight_time': config.weight_time
            },
            'history': history
        }, f, indent=2)
    
    print(f"\nResults saved to {args.output}")
    return 0


if __name__ == '__main__':
    exit(main())
