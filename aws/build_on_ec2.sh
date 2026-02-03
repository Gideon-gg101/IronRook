#!/bin/bash
# Build script to run on EC2

set -e

echo "Installing build tools..."
sudo yum install -y gcc-c++ cmake make aws-cli git

echo "Downloading source..."
aws s3 cp s3://prometheus-tuning-data/prometheus_source.zip .

echo "Extracting..."
unzip -q prometheus_source.zip

echo "Building..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-march=x86-64 -mtune=generic"
make -j4

echo "Uploading binary..."
aws s3 cp IronRook s3://prometheus-tuning-data/engine/IronRook_linux

echo "Build complete!"
echo "Binary uploaded to s3://prometheus-tuning-data/engine/IronRook_linux"

# Shutdown instance
sudo shutdown -h now
