#!/bin/bash
# PureBLM2 — Universal Bare-Metal AI ISO Builder
set -euo pipefail

MODEL_IN="${1:?Usage: $0 <model.baremetallama>}"
OUT_ISO="${2:-pureblm2_out.iso}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK_DIR="$SCRIPT_DIR/pureblm2_build"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Find the best kernel source (either the one we downloaded or the default)
if [ -f "$SCRIPT_DIR/iso/boot/bzImage" ]; then
    KERNEL_SOURCE="$SCRIPT_DIR/iso/boot/bzImage"
else
    KERNEL_SOURCE="$REPO_ROOT/bzImage"
fi

echo "[*] Building PureBLM2 ISO for: $MODEL_IN"

# 1. Setup workspace
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"/{rootfs,iso/boot/grub}

# 2. Populate rootfs from template
echo "[*] Populating rootfs..."
cp -r "$REPO_ROOT/pureblm/rootfs/"* "$WORK_DIR/rootfs/"

# 3. Inject custom init
cat > "$WORK_DIR/rootfs/init" << 'INIT_EOF'
#!/bin/sh
export PATH=/usr/sbin:/usr/bin:/sbin:/bin
# NUCLEAR FIX: Force the entire rootfs to be Writable and Executable
mount -o remount,rw,exec / 2>/dev/null

mount -t devtmpfs none /dev 2>/dev/null || mount -t tmpfs none /dev

# === MANUAL CONSOLE SETUP ===
for dev in /dev/console /dev/tty0 /dev/ttyS0; do
    if [ -c "$dev" ]; then
        exec <"$dev" >"$dev" 2>&1
        break
    fi
done

echo ">>> PureBLM2: INIT SCRIPT STARTED <<<"
echo ">>> Setting up hardware..."

echo "=== PureBLM2 Booting ==="
export TERM=xterm
stty sane 2>/dev/null || true

mount -t proc none /proc
mount -t sysfs none /sys

# Prepare standard paths
mkdir -p /tmp /app/tmp
chmod 1777 /tmp /app/tmp

MODEL="/app/model.elf"
echo "=== PureBLM2 Configuration ==="
echo "Model: $MODEL"
if [ -f "$MODEL" ]; then
    echo "Model Size: $(du -h $MODEL)"
else
    echo "ERROR: Model file $MODEL not found!"
    ls -l /app
fi
echo "System RAM: $(grep MemTotal /proc/meminfo)"
echo "CPU Cores: $(nproc)"
echo "=============================="

echo ">>> LOADING AI INTO MEMORY (Please wait 10-30 seconds)..."
chmod +x "$MODEL"

# RUN NATIVE ELF DIRECTLY
exec "$MODEL" || {
    echo "AI failed to start. Dropping to shell..."
    exec /bin/sh
}
INIT_EOF
chmod +x "$WORK_DIR/rootfs/init"

# 4. Copy model (Using the NATIVE ELF)
mkdir -p "$WORK_DIR/rootfs/app"
# We look for the .elf file first, then fallback to whatever was passed
if [ -f "$REPO_ROOT/qwen2505b.elf" ]; then
    cp "$REPO_ROOT/qwen2505b.elf" "$WORK_DIR/rootfs/app/model.elf"
else
    cp "$MODEL_IN" "$WORK_DIR/rootfs/app/model.elf"
fi
chmod +x "$WORK_DIR/rootfs/app/model.elf"

# 5. Pack initramfs
echo "[*] Creating initramfs..."
(cd "$WORK_DIR/rootfs" && find . | cpio -H newc -o 2>/dev/null | gzip -1 > "../iso/boot/initramfs.img")

# 6. Prepare ISO layout
cp "$KERNEL_SOURCE" "$WORK_DIR/iso/boot/bzImage"
cat > "$WORK_DIR/iso/boot/grub/grub.cfg" << 'GRUB_EOF'
set timeout=0
set default=0

# Use high-compatibility video settings
insmod all_video
set gfxpayload=1024x768x16,1024x768

menuentry "PureBLM2 AI (Universal Hardware)" {
    linux /boot/bzImage console=ttyS0,115200 console=tty0 \
          video=efifb \
          init=/init rootfstype=ramfs \
          apparmor=0 security=none loglevel=7 \
          panic=0
    initrd /boot/initramfs.img
}
GRUB_EOF

# 7. Build ISO
echo "[*] Building ISO: $OUT_ISO"
grub-mkrescue -o "$OUT_ISO" "$WORK_DIR/iso/" 2>/dev/null

echo "[✅] SUCCESS! ISO ready at: $OUT_ISO"
echo "[*] Test with: qemu-system-x86_64 -m 2G -cdrom $OUT_ISO -nographic -enable-kvm -cpu host"
