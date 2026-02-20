Model used in tests and examples: `qwen2.5-0.5b-instruct_q4_K_M.gguf`

**HuggingFace Repo**: [provetgrizzner/qwen-bundle](https://huggingface.co/provetgrizzner/qwen-bundle)

# BareMetalLlama 🦙

A universal, OS-agnostic AI model bundler and runner. Build single-file AI executables that run natively on Linux, Windows, and macOS without dependencies, using **Cosmopolitan Libc**.

## 🚀 Key Features

- **Polyglot Binaries**: One file (`.baremetallama`) runs on Windows (as `.exe`), Linux, and macOS.
- **Embedded Inference**: The engine and weights are fused into a single executable.
- **Zero Dependencies**: No Python, no CUDA, no DLLs required on the target machine.
- **Bare-Metal Vision**: Roadmap for `.pureblm`, a bootable RTOS runner that runs AI directly on hardware.

## 📂 Project Structure

- `bundler/`: CLI tool to package models into `.baremetallama` files.
- `vendor/llama.cpp/`: Modified Llama.cpp source for Cosmopolitan compatibility.
- `Makefile.cosmo`: The primary build system for universal binaries.
- `PUREBLM_ARCHITECTURE.md`: Technical roadmap for the bare-metal bootable runner.
- `docs/diagrams.md`: Mermaid.js diagrams of the system architecture.

## 🛠️ Build Instructions

### 1. Requirements
You need the **Cosmocc** toolchain to compile universal binaries.
```bash
# Download and setup Cosmocc
wget https://cosmo.zip/pub/cosmocc/cosmocc.zip
unzip cosmocc.zip -d cosmocc/
export PATH="$PWD/cosmocc/bin:$PATH"
```

### 2. Compile the AI Engine
Use the custom Cosmopolitan Makefile to build the portable `llama-server.com`:
```bash
make -f Makefile.cosmo -j$(nproc)
```

### 3. Compile the Bundler
```bash
cosmoc++ -O3 -mcosmo bundler/bundler.cpp -o bundler/baremetallama.com
```

### 4. Create your Universal AI
Pack a GGUF model into a standalone `.baremetallama` file:
```bash
./bundler/baremetallama.com llama-server.com your_model.gguf qwen.baremetallama
```

## 🖥️ Usage

### Windows
Rename to `.exe` or run directly from CMD/PowerShell:
```powershell
.\qwen.baremetallama
```

### Linux / macOS
```bash
chmod +x qwen.baremetallama
./qwen.baremetallama
```

*By default, running the bundle without arguments launches an interactive **Chat TUI** in your terminal.*

---
**Repository**: [RedLordezh7Venom/baremetallama](https://github.com/RedLordezh7Venom/baremetallama)  
**TUI Engine**: Modified `llama-server` (Llama.cpp)  
**Runtime**: Cosmopolitan Libc