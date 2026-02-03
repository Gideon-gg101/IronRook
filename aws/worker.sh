#!/bin/bash
# AWS Worker Script - Runs on each EC2 Spot instance
# Pulls params from S3, generates games, pushes results back

set -e

WORKER_ID=$(ec2-metadata --instance-id | cut -d " " -f 2)
S3_BUCKET="s3://prometheus-tuning-data"
GAMES_PER_WORKER=3900  # 125K / 32 workers
VCPUS=$(nproc)

echo "=== Prometheus Tuning Worker ===" 
echo "Worker ID: $WORKER_ID"
echo "vCPUs: $VCPUS"
echo "Games to generate: $GAMES_PER_WORKER"
echo

# 1. Install dependencies
echo "[1/6] Installing dependencies..."
sudo yum update -y
sudo yum install -y wget aws-cli

# 2. Download engine
echo "[2/6] Downloading engine..."
aws s3 cp $S3_BUCKET/engine/IronRook_linux ./IronRook
chmod +x IronRook

# 3. Download cutechess-cli
echo "[3/6] Installing cutechess-cli..."
wget -q https://github.com/cutechess/cutechess/releases/download/1.2.0/cutechess-cli-1.2.0-linux64.tar.gz
tar -xzf cutechess*.tar.gz
chmod +x cutechess-cli

# 4. Download opening book
echo "[4/6] Downloading opening book and experience..."
aws s3 cp $S3_BUCKET/openings/8moves_v3.epd ./openings.epd
aws s3 cp $S3_BUCKET/openings/IronBook.exp ./IronBook.exp

# 5. Generate games
echo "[5/6] Generating $GAMES_PER_WORKER games..."
echo "Concurrency: $VCPUS"
echo

./cutechess-cli \
  -engine cmd=./IronRook name=IronRook option.ExperiencePath=IronBook.exp option.Nodes=3000 \
  -engine cmd=./IronRook name=IronRook option.ExperiencePath=IronBook.exp option.Nodes=3000 \
  -each proto=uci tc=inf \
  -games $GAMES_PER_WORKER \
  -concurrency $VCPUS \
  -pgnout games_${WORKER_ID}.pgn \
  -recover \
  -repeat \
  -openings file=openings.epd format=epd order=random

# 6. Upload results
echo "[6/6] Uploading results..."
aws s3 cp games_${WORKER_ID}.pgn $S3_BUCKET/phase1/games/

# Report completion
GAMES_GENERATED=$(grep -c '\[Event' games_${WORKER_ID}.pgn || echo "0")
echo "{\"worker_id\": \"$WORKER_ID\", \"games\": $GAMES_GENERATED, \"timestamp\": \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"}" > status.json
aws s3 cp status.json $S3_BUCKET/phase1/status/${WORKER_ID}.json

echo
echo "✅ Worker $WORKER_ID complete!"
echo "Generated: $GAMES_GENERATED games"
echo "Uploaded to: $S3_BUCKET/phase1/games/games_${WORKER_ID}.pgn"
echo

# Shutdown (optional - saves cost)
# sudo shutdown -h now
