# Prometheus Engine - Tuning Guide

Complete guide to parameter tuning using SPSA and Texel methods.

## Overview

The engine has **42 tunable parameters** across search and evaluation. Tuning these parameters is the **single biggest Elo gain** you can achieve (+200-400 Elo).

---

## Parameter Categories

### 1. Search Parameters (24 total)

#### Singular Extensions (5 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `SingularMargin` | 32 | 16-64 | Margin for singularity detection |
| `SingularMinDepth` | 8 | 6-12 | Minimum depth to try SE |
| `SingularDepthDelta` | 3 | 2-5 | Depth reduction for verification |
| `SingularNegExtDepth` | 6 | 4-8 | Min depth for negative extensions |
| `SingularNegExtMargin` | -20 | -40-0 | Margin for negative extensions |

**Impact**: +30-50 Elo when tuned

#### Null Move Pruning (3 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `NullMoveReduction` | 3 | 2-4 | Base reduction depth |
| `NullMoveDepthDivisor` | 4 | 3-6 | Depth-based scaling |
| `NullMoveMinDepth` | 2 | 1-4 | Minimum depth for NMP |

**Impact**: +15-30 Elo when tuned

#### Late Move Reductions (5 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `LMR_Base` | 75 | 50-120 | Base reduction (centipawns) |
| `LMR_Divisor` | 200 | 150-300 | Divisor for logarithmic formula |
| `LMR_HistoryDivisor` | 8192 | 4096-16384 | History score scaling |
| `LMR_MinDepth` | 3 | 2-5 | Minimum depth for LMR |
| `LMR_MinMoveIndex` | 2 | 1-4 | First move to reduce |

**Impact**: +40-60 Elo when tuned

#### Futility Pruning (2 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `FutilityMargin` | 120 | 80-180 | Margin per remaining depth |
| `FutilityMaxDepth` | 7 | 5-9 | Maximum depth for futility |

**Impact**: +10-20 Elo when tuned

#### Other Search (9 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `AspirationWindow` | 15 | 10-30 | Initial window size |
| `AspirationGrowth` | 40 | 30-60 | Window growth rate |
| `AspirationPanic` | 300 | 200-500 | Panic threshold to widen |
| `IID_MinDepthPV` | 4 | 3-6 | IID min depth (PV nodes) |
| `IID_MinDepthNonPV` | 8 | 6-12 | IID min depth (non-PV) |
| `ProbcutMargin` | 200 | 150-300 | Probcut beta margin |
| `ProbcutMinDepth` | 5 | 4-8 | Minimum depth for probcut |
| `MultiCutThreshold` | 3 | 2-4 | Cutoffs needed for multi-cut |
| `MultiCutMinDepth` | 4 | 3-6 | Minimum depth for multi-cut |

---

### 2. Evaluation Parameters (18 total)

#### Material (Basic)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `PawnValueMg` | 100 | 80-120 | Pawn value (midgame) |
| `PawnValueEg` | 120 | 100-150 | Pawn value (endgame) |
| `KnightValueMg` | 325 | 300-350 | Knight value (midgame) |
| `KnightValueEg` | 325 | 300-350 | Knight value (endgame) |
| `BishopValueMg` | 330 | 310-360 | Bishop value (midgame) |
| `BishopValueEg` | 330 | 310-360 | Bishop value (endgame) |
| `RookValueMg` | 500 | 480-530 | Rook value (midgame) |
| `RookValueEg` | 550 | 520-580 | Rook value (endgame) |
| `QueenValueMg` | 950 | 900-1000 | Queen value (midgame) |
| `QueenValueEg` | 1000 | 950-1050 | Queen value (endgame) |

**Impact**: +50-100 Elo when tuned (material is critical!)

#### Pawn Structure (4 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `PassedPawnBonus_6` | 120 | 80-160 | Passed pawn on 6th rank |
| `PassedPawnBonus_7` | 250 | 200-300 | Passed pawn on 7th rank |
| `DoubledPawnPenalty` | -12 | -20--5 | Doubled pawn penalty |
| `IsolatedPawnPenalty` | -15 | -25--8 | Isolated pawn penalty |

**Impact**: +20-40 Elo when tuned

#### King Safety (2 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `KingSafetyAttackWeight` | 40 | 30-60 | Attack unit weight |
| `KingSafetyDefenseWeight` | 20 | 10-30 | Defense unit bonus |

**Impact**: +30-50 Elo when tuned

#### Noise Control (2 params)
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| `LazyEvalMargin` | 150 | 100-250 | Margin for lazy evaluation |
| `MaxNonMateEval` | 29000 | 25000-32000 | Max non-mate score |

**Impact**: +10-20 Elo (stability, not strength)

---

## SPSA Tuning

### What is SPSA?
**Simultaneous Perturbation Stochastic Approximation** - A gradient-free optimization algorithm that:
1. Perturbs all parameters randomly
2. Plays games to measure performance
3. Updates parameters based on results
4. Repeats until convergence

### Running SPSA

#### 1. Configure Parameters
Edit `configs/spsa_config.json`:
```json
{
  "iterations": 100,
  "games_per_iteration": 50,
  "time_control": "10+0.1",
  "alpha": 0.602,
  "gamma": 0.101,
  "a": 100,
  "c": 5,
  "parameters": {
    "SingularMargin": {
      "value": 32,
      "min": 16,
      "max": 64,
      "step": 5
    },
    "NullMoveReduction": {
      "value": 3,
      "min": 2,
      "max": 4,
      "step": 1
    }
    // ... add more parameters
  }
}
```

#### 2. Run SPSA
```bash
# Quick test (2 hours)
python scripts/spsa_tuner.py \
    --config configs/spsa_quick.json \
    --iterations 20 \
    --games 30

# Weekly run (overnight)
python scripts/spsa_tuner.py \
    --config configs/spsa_weekly.json \
    --iterations 50 \
    --games 50

# Production run (multi-day)
python scripts/spsa_tuner.py \
    --config configs/spsa_production.json \
    --iterations 200 \
    --games 100
```

#### 3. Monitor Progress
```bash
# Check current iteration
tail -f spsa.log

# Plot convergence
python scripts/plot_spsa.py --input spsa_history.json
```

#### 4. Apply Results
```bash
# Best parameters saved automatically to:
# configs/spsa_best_params.json

# Apply to engine (via UCI)
setoption name SingularMargin value 28
setoption name NullMoveReduction value 3
# ...
```

### SPSA Best Practices

#### Parameter Selection
- ✅ **Start with search params** (bigger impact than eval)
- ✅ **Tune 5-10 params at once** (not all 42!)
- ✅ **Group related params** (all SE params together)
- ❌ **Don't tune all parameters** (overfitting risk)

#### Game Configuration
- **Time Control**: 10+0.1 (fast, noisy) or 60+0.6 (slow, accurate)
- **Games per Iteration**: 30-100 (more = less noise)
- **Opening Book**: Use diverse book to avoid overfitting

#### Convergence Criteria
- **Iterations**: 100-200 for good results
- **Elo Gain**: Stop if no improvement for 20 iterations
- **Parameter Stability**: Stop if parameters change < 5%

---

## Texel Tuning

### What is Texel Tuning?
Optimizes evaluation parameters to minimize prediction error on a large dataset of positions.

### Preparing Data

#### 1. Generate Self-Play Games
```bash
python scripts/continuous_selfplay.py \
    --games 10000 \
    --tc 10+0.1 \
    --output selfplay_games.pgn
```

#### 2. Extract Quiet Positions
```bash
python scripts/extract_positions.py \
    --input selfplay_games.pgn \
    --output quiet_positions.epd \
    --min-depth 8 \
    --max-material-diff 200
```

**Criteria for "quiet"**:
- No checks
- No captures
- Material roughly balanced
- Sufficient depth searched

#### 3. Run Texel Tuning
```bash
python scripts/texel_tuner.py \
    --positions quiet_positions.epd \
    --params configs/texel_params.json \
    --epochs 100 \
    --learning-rate 0.01
```

### Texel Configuration

`configs/texel_params.json`:
```json
{
  "parameters": [
    {
      "name": "PawnValueMg",
      "min": 80,
      "max": 120,
      "step": 2
    },
    {
      "name": "KnightValueMg",
      "min": 300,
      "max": 350,
      "step": 5
    }
    // ... evaluation params only
  ],
  "K": 1.0,  // Sigmoid scaling factor
  "lambda": 0.0,  // Regularization
  "batch_size": 1000
}
```

### Expected Results
```
Epoch 1/100: Error = 0.2543
Epoch 10/100: Error = 0.2421 (-4.8%)
Epoch 50/100: Error = 0.2315 (-9.0%)
Epoch 100/100: Error = 0.2298 (-9.6%)

Best parameters saved to: texel_best_params.json
Estimated Elo gain: +45 Elo
```

---

## Combined Tuning Strategy

### Phase 1: Material (Texel)
**Duration**: 1-2 days  
**Parameters**: Piece values (10 params)  
**Expected**: +50-100 Elo

```bash
python scripts/texel_tuner.py \
    --positions quiet_positions.epd \
    --params configs/texel_material.json \
    --epochs 100
```

### Phase 2: Search (SPSA)
**Duration**: 3-5 days  
**Parameters**: SE, NMP, LMR (13 params)  
**Expected**: +80-140 Elo

```bash
python scripts/spsa_tuner.py \
    --config configs/spsa_search.json \
    --iterations 150 \
    --games 60
```

### Phase 3: Evaluation (Texel)
**Duration**: 2-3 days  
**Parameters**: Pawn structure, king safety (8 params)  
**Expected**: +40-80 Elo

```bash
python scripts/texel_tuner.py \
    --positions quiet_positions.epd \
    --params configs/texel_eval.json \
    --epochs 100
```

### Phase 4: Fine-Tuning (SPSA)
**Duration**: 1 week  
**Parameters**: All 42 params (in batches)  
**Expected**: +20-40 Elo

```bash
python scripts/spsa_tuner.py \
    --config configs/spsa_full.json \
    --iterations 200 \
    --games 100
```

**Total Expected Gain**: +190-360 Elo

---

## Monitoring Tuning Progress

### Elo Tracking
```bash
# Plot Elo vs iteration
python scripts/plot_tuning.py \
    --input spsa_history.json \
    --output tuning_progress.png
```

### Parameter Evolution
```bash
# Track how parameters change
python scripts/analyze_params.py \
    --input spsa_history.json \
    --param SingularMargin
```

### Test Suite Validation
```bash
# Check if tuning improves test performance
python scripts/run_test_suites.py build/IronRook.exe \
    --suites tests/suites/*.epd \
    --output tuning_test_results.json
```

---

## Common Pitfalls

### 1. Overfitting
**Problem**: Parameters work well on tuning set but regress on real games  
**Solution**: 
- Use diverse opening book
- Test on independent game set
- Don't tune too many iterations

### 2. Local Minima
**Problem**: SPSA gets stuck in suboptimal solution  
**Solution**:
- Restart SPSA with different initial values
- Increase `c` parameter for more exploration
- Use simulated annealing

### 3. Noisy Gradients
**Problem**: Too few games per iteration causes random walk  
**Solution**:
- Increase games per iteration (50-100 minimum)
- Use longer time control
- Average over multiple runs

### 4. Parameter Correlation
**Problem**: Correlated params (e.g., LMR_Base and LMR_Divisor) interfere  
**Solution**:
- Tune correlated params separately
- Use multi-stage tuning
- Fix one, tune the other

---

## Hardware Requirements

### Minimal Setup
- **CPU**: 4 cores
- **RAM**: 4GB
- **Time**: 1-2 weeks for full tuning

### Recommended Setup
- **CPU**: 16+ cores
- **RAM**: 16GB
- **Time**: 3-5 days for full tuning

### Cloud Setup (AWS/GCP)
- **Instance**: c5.4xlarge (16 vCPUs)
- **Cost**: ~$0.68/hour × 100 hours = $68
- **Time**: 2-3 days for full tuning

---

## Version Control

### Tracking Parameter Changes
```bash
# Commit after each tuning phase
git add configs/params.json
git commit -m "SPSA Phase 2: Search params tuned (+82 Elo)"
git tag v1.2.1-tuned
```

### Parameter History
Keep a log: `docs/tuning_history.md`
```markdown
## 2026-01-25: SPSA Search Tuning
- Parameters: SE (5), NMP (3), LMR (5)
- Iterations: 150
- Elo gain: +82 Elo
- Best params: configs/spsa_search_best.json
```

---

## Troubleshooting

### SPSA Not Converging
```bash
# Check if gradient is too noisy
python scripts/diagnose_spsa.py --input spsa_history.json

# Solutions:
# 1. Increase games per iteration
# 2. Reduce number of parameters
# 3. Increase 'a' parameter (more weight to early iterations)
```

### Texel Error Not Decreasing
```bash
# Check position quality
python scripts/validate_positions.py --input quiet_positions.epd

# Solutions:
# 1. Re-extract positions with stricter criteria
# 2. Increase dataset size (more games)
# 3. Adjust learning rate
```

---

## Example: Tuning Session

```bash
# Day 1: Setup
python scripts/generate_selfplay.py --games 10000 > selfplay.pgn
python scripts/extract_positions.py --input selfplay.pgn

# Day 2-3: Material (Texel)
python scripts/texel_tuner.py \
    --params configs/texel_material.json \
    --epochs 100

# Day 4-8: Search (SPSA)
python scripts/spsa_tuner.py \
    --config configs/spsa_search.json \
    --iterations 150

# Day 9-11: Evaluation (Texel)
python scripts/texel_tuner.py \
    --params configs/texel_eval.json \
    --epochs 100

# Day 12: Verification
python scripts/sprt_test.py \
    build/IronRook_baseline.exe \
    build/IronRook_tuned.exe \
    --elo0 0 --elo1 5 \
    --max-games 1000

# Result: +245 Elo, PASS with 95% confidence
```

---

**Last Updated**: 2026-01-31  
**See Also**: 
- [MAINTENANCE.md](MAINTENANCE.md) - Regular tuning schedule
- [BENCHMARKING.md](BENCHMARKING.md) - Elo tracking
