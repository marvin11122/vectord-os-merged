# ═══════════════════════════════════════════════════════════════════════════════
# VECTORD OS - Akasha Kernel Container
# Holographic Knowledge Engine - Isolated GPU Environment
# ═══════════════════════════════════════════════════════════════════════════════
#
# This container provides an isolated environment for the Akasha AI system
# with direct CUDA access to the host GPU.
#
# Build: docker build -t vectord-akasha:v1 .
# Run:   docker run -it --gpus all -v ./data:/akasha/data vectord-akasha:v1
# ═══════════════════════════════════════════════════════════════════════════════

FROM nvidia/cuda:11.8.0-base-ubuntu22.04

# Set environment
ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1

# Install Python and dependencies
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    python3-numpy \
    && rm -rf /var/lib/apt/lists/*

# Install PyTorch with CUDA 11.8 support
RUN pip3 install --no-cache-dir \
    torch \
    torchvision \
    torchaudio \
    --index-url https://download.pytorch.org/whl/cu118

# Create directory structure
RUN mkdir -p /akasha/{bin,knowledge_base,data,logs}

# Copy the Akasha system files
COPY god_repl.py /akasha/bin/
COPY akasha_orchestrator.py /akasha/bin/

# Create startup script
RUN cat > /akasha/bin/start.sh << 'EOF'
#!/bin/bash
echo "╔══════════════════════════════════════════════════════════════════╗"
echo "║         VECTORD OS - AKASHA KERNEL v1.67¹¹¹                    ║"
echo "║         Holographic Knowledge Engine                           ║"
echo "╚══════════════════════════════════════════════════════════════════╝"
echo ""
echo "[INIT] Container ID: $(hostname)"
echo "[INIT] GPU Status:"

# Check for NVIDIA GPU
if command -v nvidia-smi &> /dev/null; then
    nvidia-smi --query-gpu=name,memory.total --format=csv,noheader
else
    echo "[WARN] nvidia-smi not found - GPU passthrough may not be configured"
fi

echo ""
echo "[READY] Starting Omniscience Terminal..."
echo ""

cd /akasha/data
python3 /akasha/bin/god_repl.py
EOF

RUN chmod +x /akasha/bin/start.sh

# Set working directory
WORKDIR /akasha/data

# Default command
CMD ["/akasha/bin/start.sh"]

