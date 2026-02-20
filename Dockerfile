# Use Ubuntu as the build base
FROM ubuntu:22.04

# Install build dependencies
RUN apt-get update && apt-get install -y \
    wget \
    unzip \
    make \
    g++ \
    && rm -rf /var/lib/apt/lists/*

# Install Cosmocc toolchain
WORKDIR /opt
RUN wget https://cosmo.zip/pub/cosmocc/cosmocc.zip && \
    unzip cosmocc.zip -d cosmocc

# Update PATH to include cosmocc
ENV PATH="/opt/cosmocc/bin:${PATH}"

# Setup application directory
WORKDIR /app
COPY . .

# Build the portable AI engine (llama-server.com)
RUN make -f Makefile.cosmo -j$(nproc)

# Build the bundler tool
RUN cosmoc++ -O3 -mcosmo bundler/bundler.cpp -o bundler/baremetallama.com

# Entrypoint maps directly to the bundler
# Usage: docker run -v $(pwd):/work baremetallama /work/model.gguf /work/output.baremetallama
ENTRYPOINT ["/app/bundler/baremetallama.com", "/app/llama-server.com"]
