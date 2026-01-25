# GitHub Actions SPSA Tuning - Setup Guide

## Overview

This workflow enables automated SPSA parameter tuning using GitHub Actions runners.

**Configuration:**
- **Runs:** Weekly (Sunday 2 AM) or manual trigger
- **Duration:** ~5 hours max
- **Iterations:** 20 (CI-optimized)
- **Games per iteration:** 20
- **Depth:** 8 (fast evaluation)
- **Total games:** ~400 per run

## Setup Instructions

### 1. Create GitHub Repository

If you haven't already:
```bash
cd c:\Users\Administrator\Desktop\GideonInspired\Chess2.0\Engines2.0\Prometheus
git init
git add .
git commit -m "Initial commit with SPSA CI"
git remote add origin https://github.com/YOUR_USERNAME/Prometheus.git
git push -u origin main
```

### 2. Verify File Structure

Ensure these files exist:
```
Prometheus/
├── .github/
│   └── workflows/
│       └── spsa-tuning.yml     # Workflow file
├── config_ci.json               # CI configuration
├── scripts/
│   └── spsa.py                  # SPSA optimizer
├── CMakeLists.txt
└── src/
    └── ... (engine source)
```

### 3. First Manual Run

1. Go to GitHub repository
2. Click "Actions" tab
3. Select "SPSA Tuning (CI)" workflow
4. Click "Run workflow"
5. Wait ~5 hours for completion

### 4. Check Results

After completion:
1. Go to "Actions" tab
2. Click the completed run
3. Download artifacts:
   - `spsa-params` - Best parameters found
   - `spsa-log-XXX` - Detailed tuning log

### 5. Apply Tuned Parameters

```bash
# Download params.json artifact from GitHub
# Copy to your local build directory
copy params.json build\params.json

# Load in engine via UCI
uci
setoption name LoadParams value params.json
```

## Configuration Details

### Parameters Being Tuned (9 total)

| Parameter | Range | Impact |
|-----------|-------|--------|
| LazyEvalMargin | 100-250 | Eval speed |
| NMPBaseReductionMG | 2-4 | Null move depth |
| NMPBaseReductionEG | 2-4 | Endgame null move |
| AspirationWindow | 15-40 | Search stability |
| LMRBaseReduction | 50-100 | Move reduction |
| LMRDepthDivisor | 150-300 | Depth scaling |
| IIDMinDepthPV | 4-8 | IID activation |
| IIDReductionPV | 1-3 | IID depth |
| EvalHysteresis | 0-15 | Eval smoothing |

### Time Budget

- **Build:** ~5 minutes
- **Setup:** ~2 minutes  
- **Tuning:** ~280 minutes (4.7 hours)
- **Cleanup:** ~1 minute
- **Total:** ~290 minutes

**GitHub Free Tier:**
- Public repos: 2000 min/month → **6-7 runs/month**
- Private repos: 500 min/month → **1-2 runs/month**

## Monitoring Progress

### View Live Logs

1. Click running workflow
2. Click "spsa-tune" job
3. Expand "Run SPSA Tuning (CI Mode)"
4. Watch real-time progress

### Expected Output

```
Iteration 1/20
  Running 20 games...
  Games complete: 10/20
  Games complete: 20/20
  Current best: +5.2 Elo
  Updating parameters...

Iteration 2/20
  Running 20 games...
  ...
```

## Troubleshooting

### Build Fails

Check `CMakeLists.txt` paths are relative, not absolute.

### No cutechess-cli

Workflow downloads it automatically. If it fails:
1. Download manually
2. Add to repository
3. Update workflow path

### Parameters Not Saved

Check SPSA script outputs to `spsa_output_ci/best_params.json`.

### Out of GitHub Minutes

Options:
1. Reduce iterations (10 instead of 20)
2. Run bi-weekly instead of weekly
3. Use self-hosted runner (free compute)

## Expected Results

### After 1 Month (4 runs)

- **Elo gain:** +10-25 Elo (from parameter optimization)
- **Games played:** ~1600 total
- **Parameters converged:** ~60-70%

### After 3 Months (12 runs)

- **Elo gain:** +20-40 Elo  
- **Games played:** ~4800 total
- **Parameters converged:** ~90%

## Advanced: Self-Hosted Runner

For unlimited free compute:

### Setup

1. Go to Settings → Actions → Runners
2. Click "New self-hosted runner"
3. Follow instructions for Windows
4. Keep your PC running when you want tuning

### Benefits

- ✅ Unlimited minutes
- ✅ Faster (your hardware)
- ✅ Longer runs possible
- ✅ No timeouts

### Workflow Modification

```yaml
jobs:
  spsa-tune:
    runs-on: self-hosted  # Instead of windows-latest
    # ... rest of config
```

## Integration with Local Tuning

### Strategy

**Week 1:** Local SPSA (500 iterations) starts
**Week 2-4:** GitHub Actions runs weekly  
**Week 5:** Local SPSA completes

**Compare results:**
- Use whichever parameters give better Elo
- Merge best parameters from both

### Hybrid Approach

1. **Nightly Texel** (when implemented) - Fast convergence
2. **Weekly SPSA-CI** - Verification
3. **Monthly Local SPSA** - Deep search params

## Next Steps

### Immediate
1. ✅ Push code to GitHub
2. ✅ Run first workflow manually
3. ✅ Verify parameters are saved

### Short-term (1-2 weeks)
1. ⏳ Implement Texel tuning
2. ⏳ Add daily Texel workflow
3. ⏳ Create verification workflow

### Long-term (months)
1. ⏳ Expand to 32 parameters (all tunables)
2. ⏳ Add multi-stage tuning (material → eval → search)
3. ⏳ Integrate with OpenBench for distributed testing

## Cost Analysis

### GitHub Free (Public Repo)

- **Minutes:** 2000/month
- **Runs:** 6-7 per month
- **Cost:** $0
- **Elo gain:** +10-25 over 3 months

### GitHub Pro ($4/month)

- **Minutes:** 3000/month
- **Runs:** 10 per month
- **Cost:** $4/month
- **Elo gain:** +15-35 over 3 months

### Self-Hosted

- **Minutes:** Unlimited
- **Runs:** As many as you want
- **Cost:** Electricity (~$0.50/day if running 24/7)
- **Elo gain:** Limited by time, not compute

## FAQ

**Q: Can I tune all 32 parameters at once?**  
A: Not recommended on CI. Use staged tuning (9 params → 15 params → 32 params).

**Q: How do I know tuning is working?**  
A: Check for steady Elo improvement in logs. Should see +0.5 to +2 Elo per iteration.

**Q: What if parameters get worse?**  
A: Workflow preserves previous best. You can always download earlier artifacts.

**Q: Can I use Linux runners instead?**  
A: Yes! Just change `windows-latest` to `ubuntu-latest` and adjust paths.

**Q: Should I disable scheduled runs?**  
A: Start with manual runs. Enable schedule once confident it works.

---

**Status:** Ready to run!  
**First run:** Manual trigger recommended  
**Expected result:** +2-5 Elo after first 20 iterations
