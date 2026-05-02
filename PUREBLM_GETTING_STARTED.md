# 🚀 PureBLM: Getting Started

PureBLM (Pure Bare-Metal Llama) is a minimal, bootable AI environment that runs LLMs directly on hardware using a tiny Alpine Linux base and the Cosmopolitan-powered BareMetalLlama engine.

## 🛠️ Prerequisites

Ensure you have the following tools installed on your host:
- `python3`
- `qemu-system-x86_64`
- `grub-mkrescue`, `xorriso`, `cpio`, `lz4` or `gzip`

## 📦 Step 1: Prepare the Engine
The engine must be a native ELF for maximum compatibility.
```bash
# Assimilate the APE binary into a native ELF
sh cosmocc/bin/assimilate -c llama-server.com
```

## 🔗 Step 2: Bundle your Model
Fusion the AI engine with any GGUF model into a single `.elf` binary.
```bash
# Usage: ./bundler/baremetallama.com <engine> <model.gguf> <output.elf>
sh ./bundler/baremetallama.com llama-server.com path/to/your_model.gguf my_model.elf
```

## 💿 Step 3: Build the Bootable ISO
Use the optimized automation script to package the binary into a fast-booting ISO.
```bash
python3 simple_build.py my_model.elf
```
*Output: `pureblm.iso`*

## 🚀 Step 4: Run in QEMU
Test your bootable AI instantly.
```bash
qemu-system-x86_64 -m 4G -cdrom pureblm.iso -nographic -enable-kvm
```

## 💡 Pro Tips
- **Memory**: Ensure QEMU has enough RAM (Model Size + ~512MB).
- **Speed**: The `simple_build.py` script uses a **Split Initramfs** optimization, allowing the system to boot in seconds by skipping model decompression.
