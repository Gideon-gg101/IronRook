"""
SPSA Tuner for Prometheus Chess Engine

Implements Simultaneous Perturbation Stochastic Approximation (SPSA)
to optimize engine parameters for maximum playing strength.

Based on: Spall, J. C. (1998). Implementation of the simultaneous 
perturbation algorithm for stochastic optimization.
"""

import subprocess
import json
import numpy as np
import os
import sys
from dataclasses import dataclass
from typing import List, Dict
import time

# Import game runner
from game_runner import GameRunner


@dataclass
class Parameter:
    """Represents a tunable parameter"""
    name: str
    min_val: int
    max_val: int
    value: int  # Current value
    c: float  # Perturbation size
    c_end: float  # Final perturbation size


class SPSATuner:
    """SPSA optimizer for chess engine parameters"""
    
    def __init__(self, config_path: str):
        """Initialize SPSA tuner from configuration file"""
        with open(config_path, 'r') as f:
            self.config = json.load(f)
        
        # Load parameters
        self.params = [
            Parameter(
                name=p['name'],
                min_val=p['min'],
                max_val=p['max'],
                value=p['start'],
                c=p.get('c', 1.0),
                c_end=p.get('c_end', 0.5)
            )
            for p in self.config['parameters']
        ]
        
        # SPSA hyperparameters
        spsa_settings = self.config['spsa_settings']
        self.iterations = spsa_settings['iterations']
        self.games_per_iter = spsa_settings['games_per_iteration']
        self.alpha = spsa_settings['alpha']  # Learning rate decay (typically 0.602)
        self.gamma = spsa_settings['gamma']  # Perturbation decay (typically 0.101)
        self.A = spsa_settings['A']  # Stability constant
        
        # Test settings
        test_settings = self.config['test_settings']
        self.engine_path = test_settings.get('engine_path', 'build/Prometheus.exe')
        self.baseline_path = test_settings.get('baseline_path', self.engine_path)
        self.time_control = test_settings.get('time_control', '10+0.1')
        self.opening_book = test_settings.get('opening_book', None)
        self.threads = test_settings.get('threads', 1)
        self.hash = test_settings.get('hash', 64)
        self.concurrency = test_settings.get('concurrency', 1)
        self.cutechess_path = test_settings.get('cutechess_path', None)
        
        
        # Logging
        self.log_file = 'spsa_log.txt'
        self.best_params = None
        self.best_score = -float('inf')
        
        # Initialize game runner
        self.game_runner = GameRunner(
            engine_path=self.engine_path,
            time_control=self.time_control,
            threads=self.threads,
            hash=self.hash,
            concurrency=self.concurrency,
            cutechess_path=self.cutechess_path
        )
        
        print(f"SPSA Tuner initialized with {len(self.params)} parameters")
        print(f"Will run {self.iterations} iterations, {self.games_per_iter} games each")
    
    def clamp(self, param: Parameter, value: float) -> int:
        """Clamp parameter value to valid range"""
        return max(param.min_val, min(param.max_val, int(round(value))))
    
    def get_learning_rate(self, k: int) -> float:
        """Compute learning rate a_k = a / (A + k + 1)^alpha"""
        a = 0.1  # Initial learning rate
        return a / ((self.A + k + 1) ** self.alpha)
    
    def get_perturbation_size(self, k: int, param: Parameter) -> float:
        """Compute perturbation size c_k = c / (k + 1)^gamma"""
        # Linearly interpolate between c and c_end
        progress = k / max(1, self.iterations - 1)
        c = param.c * (1 - progress) + param.c_end * progress
        return c / ((k + 1) ** self.gamma)
    
    def export_params(self, params: List[Parameter], filename: str):
        """Export parameters to JSON file"""
        param_dict = {p.name: p.value for p in params}
        with open(filename, 'w') as f:
            json.dump(param_dict, f, indent=2)
    
    def evaluate(self, params: List[Parameter]) -> float:
        """
        Evaluate parameter set by playing games against baseline
        Returns: win rate (0.0 to 1.0)
        """
        # Export parameters to temp file
        temp_params = 'temp_params.json'
        self.export_params(params, temp_params)
        
        # Run games using test_match.py or cutechess-cli
        wins, losses, draws = self.run_games(temp_params, self.games_per_iter)
        
        # Calculate score
        score = (wins + 0.5 * draws) / max(1, wins + losses + draws)
        
        # Clean up
        if os.path.exists(temp_params):
            os.remove(temp_params)
        
        return score
    
    def run_games(self, params_file: str, num_games: int) -> tuple:
        """
        Run games between tuned engine and baseline
        Returns: (wins, losses, draws)
        """
        # Create baseline params file (current default values)
        baseline_params = 'baseline_params.json'
        if not os.path.exists(baseline_params):
            # Export current baseline parameters once
            baseline_dict = {p.name: p.default_val if hasattr(p, 'default_val') else p.value 
                           for p in self.params}
            with open(baseline_params, 'w') as f:
                json.dump(baseline_dict, f, indent=2)
        
        print(f"  Running {num_games} games...")
        wins, losses, draws = self.game_runner.run_match(
            params_file, 
            baseline_params, 
            num_games
        )
        
        print(f"  Results: +{wins} -{losses} ={draws}")
        return wins, losses, draws
    
    def log(self, iteration: int, theta: List[float], score_plus: float, 
            score_minus: float, gradient: np.ndarray):
        """Log iteration results"""
        with open(self.log_file, 'a') as f:
            f.write(f"\n=== Iteration {iteration + 1}/{self.iterations} ===\n")
            f.write(f"Score+: {score_plus:.4f}, Score-: {score_minus:.4f}\n")
            f.write(f"Gradient norm: {np.linalg.norm(gradient):.4f}\n")
            f.write("Current parameters:\n")
            for param, value in zip(self.params, theta):
                f.write(f"  {param.name}: {int(value)}\n")
            f.write("\n")
        
        # Also print to console
        print(f"\nIteration {iteration + 1}/{self.iterations}")
        print(f"Score+: {score_plus:.4f}, Score-: {score_minus:.4f}")
        print(f"Gradient norm: {np.linalg.norm(gradient):.4f}")
    
    def run(self):
        """Run SPSA optimization"""
        print("\n" + "="*60)
        print("Starting SPSA Optimization")
        print("="*60)
        
        # Initialize theta (current parameter values)
        theta = np.array([float(p.value) for p in self.params])
        
        # Clear log file
        with open(self.log_file, 'w') as f:
            f.write("SPSA Tuning Log\n")
            f.write(f"Started: {time.ctime()}\n")
        
        try:
            for k in range(self.iterations):
                # Generate random perturbation vector (Bernoulli ±1)
                delta = np.random.choice([-1, 1], size=len(theta))
                
                # Compute perturbation sizes for each parameter
                c_k = np.array([self.get_perturbation_size(k, p) for p in self.params])
                
                # Perturbed parameter sets
                theta_plus = theta + c_k * delta
                theta_minus = theta - c_k * delta
                
                # Clamp to valid ranges
                for i, param in enumerate(self.params):
                    theta_plus[i] = self.clamp(param, theta_plus[i])
                    theta_minus[i] = self.clamp(param, theta_minus[i])
                
                # Update parameter objects for evaluation
                params_plus = [
                    Parameter(p.name, p.min_val, p.max_val, int(theta_plus[i]), p.c, p.c_end)
                    for i, p in enumerate(self.params)
                ]
                params_minus = [
                    Parameter(p.name, p.min_val, p.max_val, int(theta_minus[i]), p.c, p.c_end)
                    for i, p in enumerate(self.params)
                ]
                
                # Evaluate both perturbations
                score_plus = self.evaluate(params_plus)
                score_minus = self.evaluate(params_minus)
                
                # Estimate gradient
                gradient = (score_plus - score_minus) / (2 * c_k * delta)
                
                # Update parameters
                a_k = self.get_learning_rate(k)
                theta = theta + a_k * gradient
                
                # Clamp to valid ranges
                for i, param in enumerate(self.params):
                    theta[i] = self.clamp(param, theta[i])
                    param.value = int(theta[i])
                
                # Log progress
                self.log(k, theta, score_plus, score_minus, gradient)
                
                # Track best parameters
                current_score = max(score_plus, score_minus)
                if current_score > self.best_score:
                    self.best_score = current_score
                    self.best_params = [p.value for p in self.params]
                    print(f"  *** New best score: {self.best_score:.4f} ***")
                
                # Save checkpoint every 10 iterations
                if (k + 1) % 10 == 0:
                    self.export_params(self.params, f'checkpoint_iter_{k+1}.json')
        
        except KeyboardInterrupt:
            print("\n\nOptimization interrupted by user")
        
        finally:
            # Save final parameters
            self.export_params(self.params, 'final_params.json')
            if self.best_params:
                best_params_list = [
                    Parameter(p.name, p.min_val, p.max_val, self.best_params[i], p.c, p.c_end)
                    for i, p in enumerate(self.params)
                ]
                self.export_params(best_params_list, 'best_params.json')
            
            print("\n" + "="*60)
            print("SPSA Optimization Complete")
            print(f"Best score: {self.best_score:.4f}")
            print(f"Final parameters saved to: final_params.json")
            if self.best_params:
                print(f"Best parameters saved to: best_params.json")
            print("="*60)


def main():
    if len(sys.argv) < 2:
        print("Usage: python spsa_tuner.py <config.json>")
        sys.exit(1)
    
    config_path = sys.argv[1]
    if not os.path.exists(config_path):
        print(f"Error: Config file not found: {config_path}")
        sys.exit(1)
    
    tuner = SPSATuner(config_path)
    tuner.run()


if __name__ == '__main__':
    main()
