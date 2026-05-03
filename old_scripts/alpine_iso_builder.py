#!/usr/bin/env python3
import os
import shutil
import subprocess
import argparse
import struct
from pathlib import Path

# --- Alpine AI ISO Builder ---
# This script bundles a .baremetallama model with Alpine Linux into a single bootable ISO.

CWD = Path.cwd()
ROOTFS_TEMPLATE = CWD / "pureblm/rootfs"
BUILD_DIR = CWD / "build_temp"
ROOTFS_BUILD = BUILD_DIR / "rootfs"
ISO_LAYOUT = BUILD_DIR / "iso"
KERNEL_IMAGE = CWD / "bzImage"

def generate_dynamic_init():
    return f"""#!/bin/sh
# --- Alpine AI Auto-Launcher (V4) ---
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

export TERM=xterm-256color
export HOME=/root
mkdir -p /root /proc /sys /dev /tmp /mnt/iso
chmod 1777 /tmp

mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /tmp -o exec

# Load ISO/Storage modules
MOD_DIR=/lib/modules/6.6.134-0-virt/kernel
insmod $MOD_DIR/drivers/cdrom/cdrom.ko 2>/dev/null
insmod $MOD_DIR/drivers/scsi/sr_mod.ko 2>/dev/null
insmod $MOD_DIR/fs/isofs/isofs.ko 2>/dev/null

echo "--------------------------------------------------"
echo "       PureBLM AI Bootloader Starting...          "
echo "--------------------------------------------------"

# Hardware Discovery: Find the model on any block device
find_model() {{
    for dev in /dev/sr* /dev/sd* /dev/vd*; do
        [ -b "$dev" ] || continue
        echo "[*] Checking $dev..."
        mount -t iso9660 -o ro "$dev" /mnt/iso 2>/dev/null || \\
        mount -t vfat -o ro "$dev" /mnt/iso 2>/dev/null || \\
        mount -o ro "$dev" /mnt/iso 2>/dev/null
        
        if [ -f "/mnt/iso/app/"*.baremetallama ]; then
            BUNDLE=$(ls /mnt/iso/app/*.baremetallama | head -n 1)
            echo "[+] FOUND MODEL: $BUNDLE on $dev"
            return 0
        fi
        umount /mnt/iso 2>/dev/null
    done
    return 1
}}

if ! find_model; then
    echo "[!] ERROR: Could not find AI model on any disk."
    echo "[*] Available devices:"
    ls /dev/s* /dev/v* /dev/sr* 2>/dev/null
    echo "[*] Dropping to shell for manual check."
    sh
fi

echo "[*] Loading AI Engine into RAM (this may take a moment)..."
cp "$BUNDLE" /tmp/ai_engine

# CRITICAL FIX for Alpine/BusyBox:
# BusyBox 'env' doesn't fallback to 'sh' on ENOEXEC.
# We patch the APE loader script to explicitly use 'sh' for the inner payload.
sed -i 's/"$TMPBIN" "$@"/sh "$TMPBIN" "$@"/g' /tmp/ai_engine

chmod +x /tmp/ai_engine

clear
echo "--------------------------------------------------"
echo "   Launching BareMetalLlama on Alpine Linux"
echo "--------------------------------------------------"

# Run the AI directly on the current TTY
/tmp/ai_engine

echo ""
echo "[!] AI Engine exited. Dropping to shell."
sh
"""

def run(cmd, cwd=None):
    subprocess.run(cmd, shell=True, check=True, cwd=cwd)

def setup_build_dir():
    if BUILD_DIR.exists():
        shutil.rmtree(BUILD_DIR)
    BUILD_DIR.mkdir()
    ROOTFS_BUILD.mkdir()
    ISO_LAYOUT.mkdir(parents=True)

def prepare_rootfs():
    print(f"[*] Preparing Alpine rootfs...")
    # Copy template
    shutil.copytree(ROOTFS_TEMPLATE, ROOTFS_BUILD, dirs_exist_ok=True, symlinks=True)
    
    # Decompress kernel modules needed for booting
    mod_path = ROOTFS_BUILD / "lib/modules/6.6.134-0-virt/kernel"
    modules = [
        "drivers/cdrom/cdrom.ko.gz",
        "drivers/scsi/sr_mod.ko.gz",
        "fs/isofs/isofs.ko.gz"
    ]
    for m in modules:
        m_full = mod_path / m
        if m_full.exists():
            print(f"[+] Decompressing {m}")
            run(f"gunzip -f {m_full}")
    
    # Inject our dynamic init
    print("[+] Injecting dynamic init...")
    init_path = ROOTFS_BUILD / "init"
    init_path.write_text(generate_dynamic_init())
    os.chmod(init_path, 0o755)

def build_initrd():
    print("[*] Packaging initrd.img...")
    output = ISO_LAYOUT / "boot" / "initrd.img"
    output.parent.mkdir(parents=True, exist_ok=True)
    # Package rootfs
    run(f"find . | cpio -o -H newc | gzip -9 > {output}", cwd=ROOTFS_BUILD)

def prepare_iso_layout(model_path):
    print(f"[*] Setting up ISO layout with model: {model_path}")
    
    # 1. Kernel
    if not KERNEL_IMAGE.exists():
        raise Exception(f"Kernel {KERNEL_IMAGE} not found!")
    shutil.copy2(KERNEL_IMAGE, ISO_LAYOUT / "boot" / "vmlinuz")
    
    # 2. Model
    app_dir = ISO_LAYOUT / "app"
    app_dir.mkdir(parents=True)
    shutil.copy2(model_path, app_dir / Path(model_path).name)
    
    # 3. GRUB Config
    grub_dir = ISO_LAYOUT / "boot" / "grub"
    grub_dir.mkdir(parents=True)
    grub_cfg = """
set default=0
set timeout=1
menuentry "Alpine AI (BareMetalLlama)" {
    linux /boot/vmlinuz quiet loglevel=3
    initrd /boot/initrd.img
}
"""
    (grub_dir / "grub.cfg").write_text(grub_cfg)

def create_iso(output_name):
    print(f"[*] Generating final ISO: {output_name}...")
    run(f"grub-mkrescue -o {output_name} {ISO_LAYOUT}")

def main():
    parser = argparse.ArgumentParser(description="Build an Alpine-based AI ISO")
    parser.add_argument("model", help="Path to the .baremetallama model file")
    parser.add_argument("-o", "--output", default="alpine_ai.iso", help="Output ISO filename")
    args = parser.parse_args()

    try:
        setup_build_dir()
        prepare_rootfs()
        build_initrd()
        prepare_iso_layout(args.model)
        create_iso(args.output)
        print(f"\\n[✅] SUCCESS! Your portable AI ISO is ready: {args.output}")
        print(f"Test it with: qemu-system-x86_64 -m 4G -cdrom {args.output}")
    except Exception as e:
        print(f"\\n[❌] ERROR: {e}")

if __name__ == "__main__":
    main()
