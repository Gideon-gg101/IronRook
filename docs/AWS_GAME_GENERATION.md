# AWS Game Generation Guide

## Quick Start (2 Instances, 11 Days, ~$376)

### Prerequisites
- AWS Account with billing enabled
- AWS CLI configured locally
- S3 bucket: `prometheus-tuning-data`

---

## Step 1: Build Linux Engine

```bash
# On your Windows PC with WSL or Docker
docker run --rm -v $(pwd):/work -w /work ubuntu:22.04 bash -c "
  apt update && apt install -y build-essential cmake git
  mkdir -p build_linux && cd build_linux
  cmake .. -DCMAKE_BUILD_TYPE=Release
  make -j8
"

# Upload to S3
aws s3 cp build_linux/IronRook s3://prometheus-deployment/IronRook_linux
```

---

## Step 2: Launch EC2 Instances

### Option A: AWS Console
1. Go to EC2 → Launch Instance
2. **AMI**: Ubuntu Server 22.04 LTS
3. **Instance type**: `c7i.2xlarge` (8 vCPUs)
4. **Number**: 2 instances
5. **Storage**: 100 GB gp3
6. **Security group**: Default (no ingress needed)
7. **IAM role**: Create with S3 full access
8. **Key pair**: Create or select existing

### Option B: AWS CLI
```bash
aws ec2 run-instances \
  --image-id ami-0c55b159cbfafe1f0 \
  --instance-type c7i.2xlarge \
  --count 2 \
  --block-device-mappings DeviceName=/dev/sda1,Ebs={VolumeSize=100} \
  --iam-instance-profile Name=S3FullAccess \
  --tag-specifications 'ResourceType=instance,Tags=[{Key=Name,Value=Prometheus-Games}]'
```

---

## Step 3: Setup Instances

**SSH into each instance**:
```bash
# Instance 1
ssh -i your-key.pem ubuntu@<instance-1-ip>

# Instance 2  
ssh -i your-key.pem ubuntu@<instance-2-ip>
```

**Run setup on both**:
```bash
wget https://raw.githubusercontent.com/your-repo/prometheus/main/aws/setup_instance.sh
chmod +x setup_instance.sh
./setup_instance.sh
```

---

## Step 4: Start Generation

**Instance 1**:
```bash
screen -S games
cd ~/prometheus-tuning
./generate_games.sh 87500 games_instance_1.pgn
# Press Ctrl+A then D to detach
```

**Instance 2**:
```bash
screen -S games
cd ~/prometheus-tuning
./generate_games.sh 87500 games_instance_2.pgn
# Press Ctrl+A then D to detach
```

---

## Step 5: Monitor Progress

**Check from local PC**:
```bash
# SSH and check
ssh ubuntu@<instance-ip> "screen -r games"
# Or count games
ssh ubuntu@<instance-ip> "grep -c '\[Event ' ~/prometheus-tuning/games_*.pgn"
```

**Expected timeline**:
- **Hour 1**: ~160 games
- **Day 1**: ~3,840 games
- **Day 5**: ~19,200 games
- **Day 11**: 87,500 games ✅

---

## Step 6: Download Results

**After completion (~11 days)**:
```bash
# Download from S3
aws s3 sync s3://prometheus-tuning-data/phase1/ Games/spsa_phase1/

# Merge PGN files
cd Games/spsa_phase1
cat games_instance_1.pgn games_instance_2.pgn > raw_games.pgn

# Verify count
grep -c '\[Event ' raw_games.pgn
# Should show: 175000
```

---

## Cost Management

### Monitor Costs
```bash
# Check AWS billing daily
aws ce get-cost-and-usage \
  --time-period Start=2026-01-31,End=2026-02-01 \
  --granularity DAILY \
  --metrics BlendedCost
```

### Set Billing Alert
1. Go to AWS Billing → Budgets
2. Create budget: $400 monthly
3. Alert at 80% ($320)

### Emergency Stop
```bash
# Stop instances immediately
aws ec2 stop-instances --instance-ids i-xxxxx i-yyyyy

# Download partial results
aws s3 sync s3://prometheus-tuning-data/phase1/ Games/spsa_phase1/
```

---

## Cost Breakdown

| Item | Calculation | Cost |
|------|-------------|------|
| **2× c7i.2xlarge** | 273h × 2 × $0.34/h | $185.64 |
| **EBS (200 GB)** | 0.08/GB/mo × 200 × 0.37 | $5.92 |
| **S3 storage** | 14 GB × $0.023/GB | $0.32 |
| **S3 transfer** | 14 GB × $0.09/GB | $1.26 |
| **Total** | | **~$193** |

**Actual timeline**: ~11 days per instance (both run in parallel)

---

## Troubleshooting

### Instance Out of Memory
```bash
# Reduce concurrency
./generate_games.sh 87500 games.pgn 6  # Use 6 instead of 8
```

### Slow Generation
```bash
# Check CPU usage
top
# Should show 8 IronRook processes at ~100% each
```

### S3 Upload Fails
```bash
# Check IAM permissions
aws sts get-caller-identity
# Manually upload
aws s3 cp games_instance_1.pgn s3://prometheus-tuning-data/phase1/
```

---

## Alternative: Spot Instances (Save 70%)

**Use spot instances for ~$55 total**:
```bash
aws ec2 request-spot-instances \
  --spot-price "0.15" \
  --instance-count 2 \
  --type "one-time" \
  --launch-specification file://spot-config.json
```

**Risk**: May be terminated if spot price spikes. Use checkpointing.

---

## Post-Deployment Cleanup

```bash
# Terminate instances
aws ec2 terminate-instances --instance-ids i-xxxxx i-yyyyy

# Delete S3 files (after downloading)
aws s3 rm s3://prometheus-tuning-data/phase1/ --recursive

# Verify no running instances
aws ec2 describe-instances --filters "Name=tag:Name,Values=Prometheus-Games"
```

---

## Timeline Summary

**Setup**: 30 minutes
**Generation**: 11 days (parallel)
**Download**: 2 hours (14 GB)
**Total**: ~11.5 days

**vs Local PC**: 73 days → **62 days saved!**

---

**Ready to deploy?** Run the setup script on AWS instances!
