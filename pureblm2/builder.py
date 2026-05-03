#!/usr/bin/env python3
import subprocess
import os
import sys
import argparse
from pathlib import Path

def check_requirements():
    """Ensure the system has the necessary tools for ISO building."""
    tools = ["grub-mkrescue", "xorriso", "cpio", "gzip"]
    missing = []
    for tool in tools:
        if subprocess.call(["which", tool], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) != 0:
            missing.append(tool)
    
    if missing:
        print(f"\033[91mError: Missing system tools: {', '.join(missing)}\033[0m")
        print("Please install them using your package manager (e.g., sudo apt install grub-common xorriso cpio)")
        sys.exit(1)

def build_iso(model_path, output_iso):
    """Wraps the build.sh script for Pythonic execution."""
    script_path = Path(__file__).parent / "build.sh"
    
    if not script_path.exists():
        print(f"\033[91mError: {script_path} not found.\033[0m")
        return

    print(f"\033[94m[*] Starting PureBLM2 Build Pipeline...\033[0m")
    print(f"\033[94m[*] Model: {model_path}\033[0m")
    print(f"\033[94m[*] Output: {output_iso}\033[0m")
    print("-" * 40)

    try:
        # We run the shell script as it contains the refined initramfs logic
        process = subprocess.Popen(
            [str(script_path), str(Path(model_path).absolute()), output_iso],
            cwd=str(script_path.parent),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True
        )

        for line in process.stdout:
            print(f"  {line.strip()}")

        process.wait()

        if process.returncode == 0:
            print("-" * 40)
            print(f"\033[92m[✅] SUCCESS: {output_iso} is ready to flash!\033[0m")
            print(f"\033[92m[*] Flash command: sudo dd if={output_iso} of=/dev/sdX status=progress\033[0m")
        else:
            print(f"\033[91m[❌] Build failed with exit code {process.returncode}\033[0m")

    except KeyboardInterrupt:
        print("\n\033[93m[!] Build cancelled by user.\033[0m")
        sys.exit(1)

def main():
    parser = argparse.ArgumentParser(description="PureBLM2: The Pure Bare Metal AI ISO Builder")
    parser.add_argument("model", help="Path to the .baremetallama or .elf file")
    parser.add_argument("-o", "--output", help="Name of the output ISO", default="pureblm2_ai.iso")
    
    args = parser.parse_args()

    if not os.path.exists(args.model):
        print(f"\033[91mError: Model file '{args.model}' not found.\033[0m")
        sys.exit(1)

    check_requirements()
    build_iso(args.model, args.output)

if __name__ == "__main__":
    main()
