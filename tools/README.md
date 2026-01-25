# SPSA Tuning Tools

## Overview
This directory contains tools for automated parameter tuning using SPSA (Simultaneous Perturbation Stochastic Approximation).

## Files

### Core Scripts
- **`spsa_tuner.py`** - Main SPSA optimizer implementing Spall's algorithm
- **`game_runner.py`** - Game playing engine supporting cutechess-cli and internal mode
- **`spsa_config.json`** - Production configuration (10 params, 500 iterations)
- **`test_config.json`** - Test configuration (3 params, 10 iterations)

## Quick Start

### 1. Test Run (Recommended First)
Test the system with a small configuration:
```bash
cd tools
python spsa_tuner.py test_config.json
```

This will:
- Tune 3 parameters over 10 iterations
- Run 20 games per iteration (200 games total)
- Take approximately 20-30 minutes
- Save results to `final_params.json` and `best_params.json`

### 2. Full Production Run
Once testing is successful:
```bash
python spsa_tuner.py spsa_config.json
```

This will:
- Tune 10 parameters over 500 iterations
- Run 100 games per iteration (50,000 games total)
- Take approximately 1-2 weeks
- Create checkpoints every 10 iterations

### 3. Apply Tuned Parameters
```bash
cd ../build
echo "importparams ../tools/best_params.json" | .\Prometheus.exe
```

## Configuration Format

```json
{
  "parameters": [
    {
      "name": "BishopPairMG",
      "min": 0,
      "max": 50,
      "start": 20,
      "c": 2.0,        // Initial perturbation size
      "c_end": 1.0     // Final perturbation size
    }
  ],
  "spsa_settings": {
    "iterations": 500,
    "games_per_iteration": 100,
    "alpha": 0.602,  // Learning rate decay (Spall's optimal)
    "gamma": 0.101,  // Perturbation decay (Spall's optimal)
    "A": 100         // Stability constant (~10% of iterations)
  },
  "test_settings": {
    "engine_path": "../build/Prometheus.exe",
    "time_control": "10+0.1",  // 10 seconds + 0.1s increment
    "threads": 1,
    "hash": 64
  }
}
```

## Game Runner Modes

### Cutechess-CLI (Recommended)
If `cutechess-cli` is installed and in PATH, it will be used automatically:
- More robust handling of crashes and illegal moves
- Professional adjudication
- Automatic opening book support
- PGN output for analysis

### Internal Mode (Fallback)
If cutechess-cli is not available:
- Pure Python UCI communication
- Basic game loop with time control
- Simpler but functional
- Good for testing

## Output Files

### During Tuning
- `spsa_log.txt` - Detailed iteration log with gradients and scores
- `baseline_params.json` - Baseline parameter set
- `checkpoint_iter_N.json` - Checkpoints every 10 iterations
- `temp_params.json` - Temporary file (auto-deleted)

### After Tuning
- `final_params.json` - Final parameter values
- `best_params.json` - Best parameter values seen during tuning
- `games.pgn` - Game records (if cutechess-cli used)

## Monitoring Progress

Watch the log file in real-time:
```bash
Get-Content spsa_log.txt -Wait  # PowerShell
```

Key metrics to monitor:
- **Score+/Score-**: Win rates for perturbed configurations
- **Gradient norm**: Should decrease over time (convergence)
- **Best score**: Track overall improvement

## Interrupting and Resuming

### Safe Interrupt
Press `Ctrl+C` to stop tuning gracefully. The tuner will:
- Save current parameters to `final_params.json`
- Save best parameters to `best_params.json`
- Complete the current game batch

### Resuming
To resume from a checkpoint:
1. Copy `checkpoint_iter_N.json` to a new config
2. Update the `start` values in the config
3. Reduce `iterations` by N
4. Run `python spsa_tuner.py resumed_config.json`

## Troubleshooting

### "cutechess-cli not found"
- Install from: https://github.com/cutechess/cutechess
- OR let it use internal mode (slower but works)

### "Engine not found"
- Check `engine_path` in config points to `build/Prometheus.exe`
- Make sure engine is built: `cd build && ninja`

### Tuning is very slow
- Reduce `games_per_iteration` (e.g., 50 instead of 100)
- Use faster time control (e.g., "5+0.05" instead of "10+0.1")
- Test first with `test_config.json`

### No improvement in scores
- Run longer (SPSA can be slow to converge)
- Check if parameters are at min/max bounds (need wider ranges)
- Verify gradient norm is decreasing over time

## Expected Results

### Successful Tuning Signs:
- Gradient norm decreases over iterations
- Best score improves over time (even if slowly)
- Parameters converge to stable values
- Final Elo gain: +150 to +400

### Problem Signs:
- Gradient norm stays constant or increases
- Parameters oscillate wildly
- Scores don't improve after 100+ iterations
- → Try reducing learning rate or perturbation sizes

## Advanced Usage

### Tuning Specific Parameters Only
Edit config to include only parameters you want to tune. Others remain at default.

### Multi-Stage Tuning
1. Tune evaluation parameters first (smoother landscape)
2. Then tune search parameters (harder landscape)
3. Finally, fine-tune all together

### Distributed Tuning
Run multiple tuning sessions in parallel with different random seeds, then compare best results.

## References
- Spall, J. C. (1998). "Implementation of the simultaneous perturbation algorithm for stochastic optimization"
- Stockfish tuning methodology: https://github.com/official-stockfish/Stockfish/wiki/Regression-Tests
