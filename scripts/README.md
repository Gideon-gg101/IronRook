# Advanced Tuning Infrastructure

This directory contains scripts for advanced tuning and testing infrastructure.

## Scripts

### `sprt_test.py`
**Purpose**: Automated regression testing using Sequential Probability Ratio Test (SPRT).

**Usage**:
```bash
python scripts/sprt_test.py baseline_engine.exe candidate_engine.exe \
    --elo0 0 --elo1 5 \
    --alpha 0.05 --beta 0.05 \
    --tc 10+0.1 \
    --book opening_book.pgn
```

**Parameters**:
- `--elo0`: H0 Elo threshold (default: 0, null hypothesis)
- `--elo1`: H1 Elo threshold (default: 5, alternative hypothesis)
- `--alpha`: Type I error rate (default: 0.05)
- `--beta`: Type II error rate (default: 0.05)
- `--tc`: Time control (e.g., `10+0.1` = 10s + 0.1s increment)
- `--book`: Opening book (PGN format)
- `--max-games`: Maximum games before timeout (default: 10000)

**Output**:
- Exit code 0: PASS (candidate is an improvement)
- Exit code 1: FAIL (candidate regressed)
- Exit code 2: TIMEOUT (inconclusive)

**Requirements**:
- `cutechess-cli` must be installed and in PATH

---

### `sensitivity_analysis.py`
**Purpose**: Rank parameters by their impact on Elo.

**Usage**:
```bash
python scripts/sensitivity_analysis.py build/IronRook.exe \
    --quick \
    --output sensitivity_results.json \
    --top-n 20
```

**Parameters**:
- `--quick`: Quick mode (fewer games, faster but less accurate)
- `--parallel`: Run tests in parallel (4 workers)
- `--output`: Output JSON file (default: `sensitivity_results.json`)
- `--top-n`: Number of top parameters to display (default: 20)

**Output**:
JSON file with sensitivity rankings:
```json
[
  {
    "name": "PawnValue",
    "baseline_value": 100,
    "elo_delta_positive": 12.5,
    "elo_delta_negative": -11.8,
    "sensitivity_score": 12.15
  },
  ...
]
```

**Use Case**:
- Identify high-impact parameters for focused SPSA tuning
- Remove low-impact parameters to reduce tuning time

---

## CI/CD Integration

### GitHub Actions Workflow
Example `.github/workflows/sprt_test.yml`:

```yaml
name: SPRT Regression Test

on: [push, pull_request]

jobs:
  sprt:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Build baseline
        run: |
          cmake -B build_baseline .
          cmake --build build_baseline --config Release
      
      - name: Build candidate
        run: |
          cmake -B build_candidate .
          cmake --build build_candidate --config Release
      
      - name: Run SPRT
        run: |
          pip install cutechess
          python scripts/sprt_test.py \
            build_baseline/IronRook \
            build_candidate/IronRook \
            --elo0 -5 --elo1 0 \
            --tc 5+0.05 \
            --max-games 500
      
      - name: Check result
        run: |
          if [ $? -eq 1 ]; then
            echo "::error::Regression detected!"
            exit 1
          fi
```

---

## Best Practices

1. **SPRT for Regression Testing**:
   - Run SPRT on every code change to catch regressions early
   - Use conservative bounds (elo0=-5, elo1=0) to reject harmful changes

2. **Sensitivity Analysis**:
   - Run periodically (monthly) to identify parameter importance drift
   - Focus SPSA tuning on top 10-15 parameters

3. **Time Controls**:
   - Use fast TC (5+0.05) for quick feedback
   - Use standard TC (10+0.1) for accurate Elo estimates

4. **Opening Books**:
   - Use diverse opening book to avoid overfitting
   - Recommended: UHO opening suite or Hert500

---

## Future Enhancements
- Multi-objective SPSA (Elo + Nodes + Time)
- OpenBench worker integration
- Parallel SPRT testing on AWS
