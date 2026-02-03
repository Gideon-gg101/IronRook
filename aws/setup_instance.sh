#!/bin/bash
# AWS EC2 Instance Setup for Game Generation
# Run this script on a fresh Ubuntu 22.04 instance

set -e

echo "=== Prometheus Game Generation - AWS Setup ==="
echo

# Update system
echo "1. Updating system..."
sudo apt update
sudo apt upgrade -y

# Install dependencies
echo "2. Installing dependencies..."
sudo apt install -y \
    build-essential \
    cmake \
    git \
    wget \
    unzip \
    awscli \
    screen

# Install cutechess-cli
echo "3. Installing cutechess-cli..."
cd /tmp
wget https://github.com/cutechess/cutechess/releases/download/1.2.0/cutechess-cli-1.2.0-linux64.tar.gz
tar -xzf cutechess-cli-1.2.0-linux64.tar.gz
sudo mv cutechess-cli /usr/local/bin/
sudo chmod +x /usr/local/bin/cutechess-cli

# Create working directory
echo "4. Creating working directory..."
mkdir -p ~/prometheus-tuning
cd ~/prometheus-tuning

# Download engine binary from S3
echo "5. Downloading engine..."
aws s3 cp s3://prometheus-deployment/IronRook_linux ~/prometheus-tuning/IronRook
chmod +x IronRook

# Verify engine
echo "6. Verifying engine..."
echo "uci" | ./IronRook | head -n 5

# Download generation script
echo "7. Setting up generation script..."
cat > generate_games.sh << 'EOF'
#!/bin/bash
# Game generation script

GAMES=${1:-87500}
OUTPUT=${2:-games_output.pgn}
NODES=3000
CONCURRENCY=8

echo "Starting game generation..."
echo "Games: $GAMES"
echo "Nodes/move: $NODES"
echo "Concurrency: $CONCURRENCY"
echo "Output: $OUTPUT"
echo

cutechess-cli \
  -engine cmd=./IronRook name=IronRook \
  -engine cmd=./IronRook name=IronRook \
  -each proto=uci tc=inf/3000 \
  -games $GAMES \
  -concurrency $CONCURRENCY \
  -pgnout $OUTPUT \
  -recover \
  -repeat

echo "Generation complete!"
aws s3 cp $OUTPUT s3://prometheus-tuning-data/phase1/$OUTPUT
echo "Uploaded to S3!"
EOF

chmod +x generate_games.sh

echo
echo "=== Setup Complete! ==="
echo
echo "To start generation:"
echo "  screen -S games"
echo "  ./generate_games.sh 87500 games_instance_1.pgn"
echo "  Press Ctrl+A then D to detach"
echo
echo "To check progress:"
echo "  screen -r games"
echo "  OR: tail -f games_instance_1.pgn | grep -c '\\[Event '"
echo
