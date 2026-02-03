# Use Ubuntu as the base image for broad compatibility
FROM ubuntu:22.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# 1. Install dependencies
# - build-essential, cmake: For building the C++ engine
# - aws-cli: For uploading results to S3
# - python3: For potential scripting needs
RUN apt-get update && apt-get install -y \
    software-properties-common \
    && add-apt-repository universe \
    && apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    awscli \
    python3 \
    python3-pip \
    python3-numpy \
    cutechess \
    && rm -rf /var/lib/apt/lists/*

# 2. Set working directory
WORKDIR /app

# 3. Copy source code
COPY . /app

# 4. Build the engine
RUN mkdir -p build_docker && cd build_docker && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# 5. Move executable to a known location
RUN cp build_docker/IronRook /app/IronRook

# 6. Make scripts executable
RUN chmod +x scripts/cloud_worker.sh

# 7. Set the entrypoint
ENTRYPOINT ["/app/scripts/cloud_worker.sh"]
