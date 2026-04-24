import os
import subprocess
import shutil
import argparse
from pathlib import Path

class PureBLMBuilder:
    def __init__(self, model_path, output_iso="qwen.pureblm.iso"):
        self.cwd = Path.cwd()
        self.rootfs = self.cwd / "rootfs"
        self.iso_dir = self.cwd / "iso"
        self.model_path = Path(model_path)
        self.output_iso = output_iso
        self.kernel_path = self.find_host_kernel()

    def find_host_kernel(self):
        """Locates the current Linux kernel on the host."""
        possible_kernels = [
            Path(f"/boot/vmlinuz-{os.uname().release}"),
            Path("/boot/vmlinuz-linux"),
            Path("/boot/vmlinuz-linux-lts"),
            Path("/boot/vmlinuz")
        ]
        for k in possible_kernels:
            if k.exists():
                return k
        return None

    def setup_rootfs(self):
        """Create rootfs and inject binaries/init."""
        print(f"[*] Setting up rootfs at {self.rootfs}...")
        
        # Create structure
        dirs = ["app", "proc", "sys", "dev", "tmp", "run", "bin", "etc", "lib", "sbin", "usr"]
        for d in dirs:
            (self.rootfs / d).mkdir(parents=True, exist_ok=True)

        # Copy Base System from template
        template = self.cwd / "pureblm/rootfs"
        if template.exists():
            print("[+] Populating rootfs from template...")
            for d in ["bin", "etc", "lib", "sbin", "usr"]:
                shutil.copytree(template / d, self.rootfs / d, dirs_exist_ok=True)

        # Inject binary
        target_app = self.rootfs / "app" / "qwen.baremetallama"
        print(f"[+] Injecting binary: {self.model_path} -> {target_app}")
        shutil.copy2(self.model_path, target_app)
        os.chmod(target_app, 0o755)

        # Create init script
        init_content = f"""#!/bin/sh
mount -t proc proc /proc
mount -t sysfs sys /sys
mount -t devtmpfs dev /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts

export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
clear

echo "Starting PureBLM..."

# Ensure the app is executable
chmod +x /app/qwen.baremetallama

# Run the TUI binary
exec /app/qwen.baremetallama
"""
        init_file = self.rootfs / "init"
        init_file.write_text(init_content)
        os.chmod(init_file, 0o755)

    def build_initramfs(self):
        """Package and compress rootfs."""
        print("[*] Building initramfs.img...")
        output_path = self.cwd / "initramfs.img"
        
        # We must run cpio from inside the rootfs directory
        cmd = f"find . | cpio -o -H newc | gzip -9 > {output_path}"
        subprocess.run(cmd, shell=True, cwd=self.rootfs, check=True)
        print(f"[+] Created: {output_path}")

    def setup_iso_structure(self):
        """Prepare ISO directory layout."""
        print("[*] Preparing ISO structure...")
        grub_dir = self.iso_dir / "boot" / "grub"
        grub_dir.mkdir(parents=True, exist_ok=True)

        # Copy Kernel (bzImage)
        if not self.kernel_path:
            raise Exception("Could not locate host kernel. Please specify path.")
        shutil.copy2(self.kernel_path, self.iso_dir / "bzImage")

        # Copy Initramfs
        shutil.copy2(self.cwd / "initramfs.img", self.iso_dir / "initramfs.img")

        # Create grub.cfg
        grub_cfg = """
set default=0
set timeout=5

menuentry "PureBLM AI Runner" {
    linux /bzImage quiet loglevel=3
    initrd /initramfs.img
}
"""
        (grub_dir / "grub.cfg").write_text(grub_cfg)

    def build_iso(self):
        """Generate the final ISO."""
        print(f"[*] Generating ISO: {self.output_iso}...")
        try:
            subprocess.run([
                "grub-mkrescue", "-o", self.output_iso, str(self.iso_dir)
            ], check=True)
            print(f"[✅] Success! ISO created: {self.output_iso}")
        except Exception as e:
            print(f"[❌] Error creating ISO: {e}")

def main():
    parser = argparse.ArgumentParser(description="PureBLM Bootable ISO Builder")
    parser.add_argument("model", help="Path to the .baremetallama binary to bundle")
    parser.add_argument("--output", default="qwen.pureblm.iso", help="Name of the output ISO")
    parser.add_argument("--skip-rootfs", action="store_true", help="Skip rootfs creation")
    
    args = parser.parse_args()
    
    builder = PureBLMBuilder(args.model, args.output)
    
    try:
        if not args.skip_rootfs:
            builder.setup_rootfs()
        builder.build_initramfs()
        builder.setup_iso_structure()
        builder.build_iso()
    except Exception as e:
        print(f"[❌] Build failed: {e}")

if __name__ == "__main__":
    main()
