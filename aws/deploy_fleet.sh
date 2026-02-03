#!/bin/bash
# Deploy 32 Spot Workers for Phase 1 SPSA
# Industry-standard configuration: c-series, capacity-optimized
# Cost: ~$220 for 125K games (8-10 days)

set -e

echo "=== Prometheus Phase 1 - Spot Fleet Deployment ==="
echo

# Configuration
WORKERS=32
GAMES_TOTAL=125000
GAMES_PER_WORKER=$((GAMES_TOTAL / WORKERS))

echo "Configuration:"
echo "  Workers: $WORKERS"
echo "  Total games: $GAMES_TOTAL"
echo "  Games per worker: $GAMES_PER_WORKER"
echo "  Estimated time: 8-10 days"
echo "  Estimated cost: ~$220"
echo

# Step 1: Create S3 bucket structure
echo "[1/7] Setting up S3..."
aws s3 mb s3://prometheus-tuning-data 2>/dev/null || echo "Bucket exists"
aws s3api put-object --bucket prometheus-tuning-data --key phase1/games/
aws s3api put-object --bucket prometheus-tuning-data --key phase1/status/
aws s3api put-object --bucket prometheus-tuning-data --key engine/
aws s3api put-object --bucket prometheus-tuning-data --key openings/

# Step 2: Upload engine
echo "[2/7] Uploading engine..."
aws s3 cp ../build/IronRook_linux s3://prometheus-tuning-data/engine/IronRook_linux

# Step 3: Upload opening book
echo "[3/7] Uploading opening book..."
aws s3 cp ../data/books/8moves_v3.epd s3://prometheus-tuning-data/openings/8moves_v3.epd

# Step 4: Encode worker script for user-data
echo "[4/7] Preparing worker script..."
WORKER_SCRIPT_B64=$(base64 -w 0 worker.sh)
cat launch_template.json | jq ".LaunchTemplateData.UserData = \"$WORKER_SCRIPT_B64\"" > launch_template_final.json

# Step 5: Create launch template
echo "[5/7] Creating launch template..."
aws ec2 create-launch-template --cli-input-json file://launch_template_final.json 2>/dev/null || \
  aws ec2 create-launch-template-version --cli-input-json file://launch_template_final.json

# Get launch template ID
TEMPLATE_ID=$(aws ec2 describe-launch-templates --launch-template-names prometheus-phase1-worker --query 'LaunchTemplates[0].LaunchTemplateId' --output text)
echo "Launch template ID: $TEMPLATE_ID"

# Step 6: Update spot fleet config with template ID
cat spot_fleet_config.json | jq ".LaunchTemplateConfigs[0].LaunchTemplateSpecification.LaunchTemplateId = \"$TEMPLATE_ID\"" > spot_fleet_config_final.json

# Step 7: Request spot fleet
echo "[6/7] Launching spot fleet..."
FLEET_ID=$(aws ec2 request-spot-fleet --spot-fleet-request-config file://spot_fleet_config_final.json --query 'SpotFleetRequestId' --output text)

echo
echo "✅ Spot fleet launched!"
echo
echo "Fleet ID: $FLEET_ID"
echo
echo "Monitor progress:"
echo "  aws ec2 describe-spot-fleet-instances --spot-fleet-request-id $FLEET_ID"
echo
echo "Check worker status:"
echo "  aws s3 ls s3://prometheus-tuning-data/phase1/status/"
echo
echo "Check game progress:"
echo "  aws s3 ls s3://prometheus-tuning-data/phase1/games/ | wc -l"
echo
echo "Cancel fleet:"
echo "  aws ec2 cancel-spot-fleet-requests --spot-fleet-request-ids $FLEET_ID --terminate-instances"
echo

# Save fleet ID for later
echo $FLEET_ID > fleet_id.txt
echo "Fleet ID saved to fleet_id.txt"

# Create monitoring script
cat > monitor_fleet.sh << 'EOF'
#!/bin/bash
FLEET_ID=$(cat fleet_id.txt)

echo "=== Fleet Status ==="
aws ec2 describe-spot-fleet-instances --spot-fleet-request-id $FLEET_ID --query 'ActiveInstances[*].[InstanceId,InstanceType,SpotInstanceRequestId]' --output table

echo
echo "=== Worker Status ==="
aws s3 ls s3://prometheus-tuning-data/phase1/status/ | wc -l | xargs echo "Completed workers:"

echo
echo "=== Game Files ==="
aws s3 ls s3://prometheus-tuning-data/phase1/games/ | wc -l | xargs echo "Game files uploaded:"

echo
echo "=== Estimated Progress ==="
WORKERS_DONE=$(aws s3 ls s3://prometheus-tuning-data/phase1/status/ | wc -l)
echo "Progress: $WORKERS_DONE / 32 workers"
PERCENT=$((WORKERS_DONE * 100 / 32))
echo "Completion: $PERCENT%"
EOF

chmod +x monitor_fleet.sh

echo
echo "[7/7] Deployment complete!"
echo
echo "Run ./monitor_fleet.sh to check progress"
