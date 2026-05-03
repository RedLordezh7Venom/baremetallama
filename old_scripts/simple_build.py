#!/usr/bin/env python3
import os
import shutil
import subprocess
import argparse
from pathlib import Path

# --- Simple PureBLM Builder ---
# This script automates the creation of a minimal bootable AI ISO.

ROOTFS_DIR = Path("rootfs")
ISO_DIR = Path("iso")
DEFAULT_MODEL = "qwen2505b.baremetallama"

import urllib.request

INIT_CONTENT = """#!/bin/sh
# PID 1 - PureBLM Bootstrapping
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mount -t tmpfs tmp /tmp
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts

export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
clear

echo "--------------------------------------------------"
echo " Starting PureBLM (Bare Metal AI)..."
echo "--------------------------------------------------"

# Launch the AI engine
chmod +x /app/model.com
exec /app/model.com
"""

def run(cmd, cwd=None):
    """Helper to run shell commands."""
    subprocess.run(cmd, shell=True, check=True, cwd=cwd)

def setup_rootfs(model_path):
    """Creates a clean, minimal rootfs."""
    print(f"[*] Creating minimal rootfs...")
    if ROOTFS_DIR.exists():
        shutil.rmtree(ROOTFS_DIR)
    
    # Create basic directory structure
    for d in ["app", "bin", "dev", "etc", "lib", "lib64", "proc", "sbin", "sys", "tmp", "usr", "var"]:
        (ROOTFS_DIR / d).mkdir(parents=True)

    # 1. Populate from Alpine template (if available)
    template = Path("pureblm/rootfs")
    if template.exists():
        print("[+] Copying base system from template...")
        for item in ["bin", "etc", "lib", "lib64", "sbin", "usr"]:
            src = template / item
            if src.exists():
                shutil.copytree(src, ROOTFS_DIR / item, dirs_exist_ok=True, symlinks=True)

    # 2. Inject the AI binary
    print(f"[+] Injecting model: {model_path}")
    target_model = ROOTFS_DIR / "app" / "model.com"
    shutil.copy2(model_path, target_model)
    os.chmod(target_model, 0o755)

    # 3. Create the /init script
    print("[+] Creating /init script...")
    (ROOTFS_DIR / "init").write_text(INIT_CONTENT)
    os.chmod(ROOTFS_DIR / "init", 0o755)

def create_iso(iso_name):
    """Packages everything into a bootable ISO.

    Uses a single, uncompressed initramfs archive.
    """
    print("[*] Packaging initramfs (uncompressed for reliability)...")
    run("find . | cpio -o -H newc > ../initramfs.img", cwd=ROOTFS_DIR)

    
    print("[*] Setting up ISO structure...")
    if ISO_DIR.exists():
        shutil.rmtree(ISO_DIR)
    (ISO_DIR / "boot/grub").mkdir(parents=True)
    
    # Locate a kernel to boot
    kernel = Path("/boot/vmlinuz-linux")
    if not kernel.exists():
        # Fallback: find any vmlinuz in /boot
        found = list(Path("/boot").glob("vmlinuz*"))
        kernel = found[0] if found else None
    
    if not kernel:
        raise Exception("No Linux kernel found in /boot. Please provide one.")

    print(f"[+] Using kernel: {kernel}")
    shutil.copy2(kernel, ISO_DIR / "vmlinuz")
    shutil.move("initramfs.img", ISO_DIR / "initramfs.img")

    # Generate GRUB configuration
    grub_cfg = """
set default=0
set timeout=0
menuentry "PureBLM" {
    linux /vmlinuz quiet loglevel=3 console=ttyS0
    initrd /initramfs.img
}
"""
    (ISO_DIR / "boot/grub/grub.cfg").write_text(grub_cfg)

    print(f"[*] Generating ISO: {iso_name}...")
    run(f"grub-mkrescue -o {iso_name} iso")

def main():
    parser = argparse.ArgumentParser(description="PureBLM Automator")
    parser.add_argument("model", nargs="?", default=DEFAULT_MODEL, help="Path to .baremetallama binary")
    parser.add_argument("-o", "--output", default="pureblm.iso", help="Output ISO filename")
    args = parser.parse_args()

    if not Path(args.model).exists():
        print(f"[!] Error: {args.model} not found.")
        return

    try:
        setup_rootfs(args.model)
        create_iso(args.output)
        print(f"\n[✅] SUCCESS: {args.output} is ready to boot!")
    except Exception as e:
        print(f"\n[❌] FAILED: {e}")

if __name__ == "__main__":
    main()


