#!/usr/bin/env python3
import os
import subprocess
import argparse
from pathlib import Path

# --- PureBLM Pipeline Automator ---
# Wraps Assimilate -> Bundle -> ISO into a single command.

def run(cmd):
    print(f"[*] Running: {cmd}")
    subprocess.run(cmd, shell=True, check=True)

def main():
    parser = argparse.ArgumentParser(description="Build a PureBLM ISO from a GGUF model.")
    parser.add_argument("gguf", help="Path to the .gguf model file")
    parser.add_argument("-o", "--output", default="pureblm.iso", help="Final ISO filename")
    parser.add_argument("--engine", default="llama-server.com", help="Path to the llama-server engine")
    args = parser.parse_args()

    gguf_path = Path(args.gguf)
    engine_path = Path(args.engine)
    temp_elf = Path("model_bundle.elf")

    if not gguf_path.exists():
        print(f"[!] Error: Model {gguf_path} not found.")
        return

    try:
        # Step 1: Assimilate Engine (only if it's not already an ELF)
        print(">>> Step 1/3: Preparing Engine (Assimilate to ELF)")
        with open(engine_path, "rb") as f:
            magic = f.read(4)
        
        if magic == b"\x7fELF":
            print(f"[i] {engine_path} is already a native ELF. Skipping assimilation.")
        else:
            run(f"sh cosmocc/bin/assimilate -c {engine_path}")

        # Step 2: Bundle GGUF into Engine

        print(">>> Step 2/3: Bundling Model into Engine")
        run(f"sh ./bundler/baremetallama.com {engine_path} {gguf_path} {temp_elf}")

        # Step 3: Generate Optimized ISO
        print(">>> Step 3/3: Generating Optimized ISO")
        run(f"python3 simple_build.py {temp_elf} -o {args.output}")

        # Cleanup
        if temp_elf.exists():
            temp_elf.unlink()

        print(f"\n[✅] COMPLETED: {args.output} is ready.")
        print(f"Run it with: qemu-system-x86_64 -m 4G -cdrom {args.output} -nographic -enable-kvm")

    except Exception as e:
        print(f"\n[❌] PIPELINE FAILED: {e}")

if __name__ == "__main__":
    main()
