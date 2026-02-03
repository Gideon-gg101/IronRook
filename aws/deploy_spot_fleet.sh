#!/bin/bash
# Launch AWS Spot Fleet for Phase 1 SPSA
# 24 workers × 4 vCPUs = 96 concurrent games
# Cost: ~$46/day, ~$575 for 125K games (13 days)

set -e

WORKERS=24
VCPUS_PER_WORKER=4
SPOT_PRICE_MAX=0.08
TOTAL_GAMES=125000
GAMES_PER_WORKER=5208  # 125000 / 24

echo "=== AWS Spot Fleet Deployment for Phase 1 ==="
echo "Workers: $WORKERS"
echo "vCPUs per worker: $VCPUS_PER_WORKER"
echo "Total concurrent games: $((WORKERS * VCPUS_PER_WORKER / 2))"
echo "Games per worker: $GAMES_PER_WORKER"
echo "Estimated cost: ~$575"
echo "Estimated time: 13 days"
echo

# Create S3 bucket if doesn't exist
echo "1. Setting up S3 bucket..."
aws s3 mb s3://prometheus-tuning-data 2>/dev/null || echo "Bucket exists"

# Upload engine binary
echo "2. Uploading engine..."
aws s3 cp ../build/IronRook s3://prometheus-deployment/IronRook_linux

# Upload opening book
echo "3. Uploading opening book..."
aws s3 cp ../data/books/8moves_v3.epd s3://prometheus-deployment/8moves_v3.epd

# Create launch template
echo "4. Creating launch template..."
aws ec2 create-launch-template \
  --launch-template-name prometheus-phase1-worker \
  --launch-template-data file://launch-template.json

# Request spot fleet
echo "5. Launching spot fleet..."
FLEET_ID=$(aws ec2 request-spot-fleet \
  --spot-fleet-request-config file://spot-fleet-config.json \
  --query 'SpotFleetRequestId' \
  --output text)

echo
echo "✅ Spot fleet launched: $FLEET_ID"
echo
echo "Monitor with:"
echo "  aws ec2 describe-spot-fleet-instances --spot-fleet-request-id $FLEET_ID"
echo
echo "Check S3 for results:"
echo "  aws s3 ls s3://prometheus-tuning-data/phase1/"
echo
echo "Cancel fleet:"
echo "  aws ec2 cancel-spot-fleet-requests --spot-fleet-request-ids $FLEET_ID --terminate-instances"
echo

# Save fleet ID
echo $FLEET_ID > fleet_id.txt
echo "Fleet ID saved to fleet_id.txt"
