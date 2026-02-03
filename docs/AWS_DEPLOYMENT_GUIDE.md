# Prometheus Tuning - AWS Deployment Guide
## For Phases 1, 2, and 3

This guide documents the **Industry Standard** workflow for generating massive chess datasets using AWS Spot Fleets. 
**Cost**: ~$2.00 per 125,000 games.
**Time**: ~80 minutes.

---

## 1. Prerequisites (Already Configured)
- **IAM Roles**: `PrometheusEC2Role` (Instance) and `aws-ec2-spot-fleet-tagging-role` (Fleet).
- **S3 Bucket**: `s3://prometheus-tuning-data`
- **Security Groups**: Default VPC with outbound access.

---

## 2. Configuration for New Phase

Before launching, update the configuration for the specific phase (e.g., Phase 2).

### A. Update Worker Script (`aws/worker.sh`)
Update the `GAMES_PER_WORKER` based on your total target:
```bash
# Example for Phase 2 (search parameters, fewer games needed?)
GAMES_PER_WORKER=2000  # Adjust based on total target / 32 workers
```

### B. Upload New Engine/Book (If changed)
If you modified the engine code:
1. **Rebuild Linux Binary**:
   ```powershell
   # Use the build instance script or Docker
   ./build.sh
   ```
2. **Upload to S3**:
   ```powershell
   aws s3 cp build_linux/IronRook s3://prometheus-tuning-data/engine/IronRook_linux
   ```

---

## 3. Deploy Fleet

Run the automated deployment script. It handles template updates and fleet requests.

```powershell
# In PowerShell
cd c:\Users\Administrator\Desktop\GideonInspired\Chess2.0\Engines2.0\Prometheus
powershell -ExecutionPolicy Bypass -File aws/deploy_fleet.ps1
```

**What this does**:
1. Encodes `aws/worker.sh` into the Launch Template.
2. Updates `prometheus-phase1-worker` launch template.
3. Requests a Spot Fleet of **32 instances** (c6i.xlarge mix).

---

## 4. Monitor Progress

### A. AWS Console (Visual)
1. **Instances**: [EC2 Console](https://console.aws.amazon.com/ec2/v2/home?region=us-east-1#Instances:) (Tag: `Prometheus-SPSA`)
2. **Fleet**: [Spot Requests](https://console.aws.amazon.com/ec2sp/v2/home?region=us-east-1#SpotFleetRequests:)
3. **Files**: [S3 Bucket](https://s3.console.aws.amazon.com/s3/buckets/prometheus-tuning-data?region=us-east-1)

### B. Command Line (Fast)
```powershell
# Check games generated (S3 file count)
aws s3 ls s3://prometheus-tuning-data/phase1/games/ | Measure-Object

# Check worker status
aws s3 ls s3://prometheus-tuning-data/phase1/status/ | Measure-Object
```

---

## 5. Retrieve Results

Once the fleet finishes (instances terminate automatically or become idle):

```powershell
# 1. Download all PGNs
aws s3 sync s3://prometheus-tuning-data/phase1/games/ Games/spsa_phase2/

# 2. Merge into single file
cd Games/spsa_phase2
Get-Content *.pgn | Set-Content raw_games.pgn
```

---

## 6. Cleanup (Important!)

**Spot Fleets must be cancelled** to stop them from maintaining capacity (e.g., restarting terminated instances).

```powershell
# Get Fleet ID
$FleetId = Get-Content fleet_id.txt

# Cancel and Terminate
aws ec2 cancel-spot-fleet-requests --spot-fleet-request-ids $FleetId --terminate-instances
```

**Verify**: Check EC2 console to ensure all instances are "Terminated".

---

## Tuning Strategy Reference

### Phase 1: Evaluation (Completed)
- **Target**: 125K - 175K games
- **Params**: Material, Mobility, Outposts
- **Config**: defaults

### Phase 2: Search (Context Dependencies)
- **Target**: 50K - 100K games (Search is noisier)
- **Params**: LMR, Null Move, Singular Extensions
- **Change**: Might need slightly different openings or TC.

### Phase 3: Fine Tuning
- **Target**: 200K+ games
- **Params**: All frozen eval terms
- **Change**: Lower learning rate, more iterations.

---

## Troubleshooting

- **"Max Spot Instance Count Exceeded"**: Request fewer workers (e.g., 16 instead of 32) in `aws/spot_fleet_config.json`.
- **"IAM Role Invalid"**: Ensure `aws-ec2-spot-fleet-tagging-role` exists (run `aws/setup_iam.ps1` if needed).
- **"S3 Access Denied"**: Check `PrometheusEC2Role` permissions.
