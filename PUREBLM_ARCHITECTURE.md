# PureBLM: The Universal Bare-Metal AI Runner

This document outlines the conceptual architecture for a truly universal, OS-agnostic AI model runner using the `.pureblm` format. Unlike the current application-level bundler, this system is designed to run directly on hardware as a self-booting "AI Unikernel."

---

## 1. The Core Concept: .pureblm
The `.pureblm` (Pure Bare-Metal Llama) is not just a package; it is a **Bootable Image**. 

While `.baremetallama` relies on a host OS (Windows/Linux/Mac) to provide drivers, memory management, and file systems, a `.pureblm` file contains its own minimal runtime environment—essentially a **Micro-RTOS** specifically tuned for tensor operations.

## 2. Theoretical Architecture

To achieve "RTOS-like" performance on bare metal, the architecture consists of three layers:

### A. The Universal Bootloader (Stage 0)
*   Supports **UEFI** (Modern PC/Mac) and **U-Boot** (Embedded/ARM/RISC-V).
*   Handles initial hardware discovery (how many CPU cores? how much RAM?).
*   Sets up the Global Descriptor Table (GDT) and switches the CPU into 64-bit Long Mode.

### B. The Micro-Kernel (Pure-Kernel)
Instead of a general-purpose OS (like Linux), this is a "Single-Address Space" kernel:
*   **No Process Switching**: The entire machine is dedicated to the inference engine. No background tasks, no interruptions.
*   **Static Memory Mapping**: The model (GGUF data) is mapped directly into the CPU's memory space at boot time.
*   **Direct Hardware Drivers**: Includes minimal drivers only for essential hardware:
    *   Serial/Video (for console output)
    *   PCIe (for potential NPU/GPU access)
    *   High-Precision Timers (for sampling)

### C. The Dedicated Inference Engine
This is a version of GGML/Llama.cpp compiled for a **freestanding environment** (no `libc`, no `std::system`).
*   Uses a custom memory allocator that treats all available RAM as one giant KV-cache.
*   Uses **Multicore Parallelization** via Inter-Processor Interrupts (IPI) rather than OS-managed threads.

## 3. The Pure-Bundler Workflow
A separate tool, the `pure-bundler`, would create the `.pureblm` file by merging:
1.  **Pure-Kernel Binary**: The compiled RTOS/Unikernel core.
2.  **Configuration**: System parameters (CPU affinity, power states).
3.  **The Model**: The quantized weights (GGUF).

**Result**: A file that can be written directly to a USB stick or flashed to an ESP32/ARM board.

## 4. Key Advantages

| Feature | .baremetallama (Current) | .pureblm (Universal Runner) |
| :--- | :--- | :--- |
| **Dependency** | Requires Host OS (Linux/Win/Mac) | **Zero Dependencies** |
| **Performance** | OS Overhead (1-5%) | **Maximum Hardware Usage** |
| **Boot Time** | Instant (after OS boot) | **< 1 second from Power-On** |
| **Security** | Subject to OS vulnerabilities | **Immutable** (No shell to hack) |
| **Format** | Polyglot Executable | **Bootable Disk Image** |

## 5. Implementation Path
To build this without harming the original bundler, we would:
1.  **Stage 1**: Use a project like **Limine** or **GRUB** to boot a minimal C++ kernel.
2.  **Stage 2**: Port the GGML "CPU Generic" backend to work without a standard library (using the `cosmocc` freestanding mode).
3.  **Stage 3**: Combine the kernel and model into the `.pureblm` format using a new `pure-bundler` utility.

*This approach turns the AI model into its own Operating System—a dedicated intelligence engine running on "Pure Metal."*

---

## Next Steps for Development

### 1. Implementing the Pure-Kernel and Engine
*   **Freestanding Toolchain**: Configure `cosmocc` or `gcc` to target a freestanding environment (`-ffreestanding -nostdlib`), removing all assumptions about an underlying OS or standard C library.
*   **GGUF-Native Memory Map**: Implement a physical memory manager that detects RAM via the BIOS/UEFI map and reserves a contiguous "Giant Page" block specifically for the model weights.
*   **Minimal HAL (Hardware Abstraction Layer)**: Write direct drivers for x86_64 interrupts (IDT) and time-keeping (TSC) to replace OS-level thread scheduling.
*   **GGML Core Decoupling**: Extract the CPU-generic tensor operations from GGML and replace `mmap`/`pthread` calls with direct memory pointers and a custom `multi-core spinlock` coordinator.
*   **Serial Console I/O**: Implement a basic UART/VGA driver to allow the AI to "type" directly into the hardware's text buffer.

**Algorithm: The Bare-Metal Boot Sequence**
1.  **Stage 0**: CPU wakes up, loads the Boot Sector, and transitions from Real Mode to 64-bit Long Mode.
2.  **Mapping**: Kernel detects total system RAM and creates a 1:1 Identity Map for the first few gigabytes.
3.  **Discovery**: Scan the `.pureblm` trailer to find the `model_offset` and `model_size` embedded by the bundler.
4.  **Allocation**: Point the GGML context directly to the hardware address where the weights are stored (Zero-Copy).
5.  **Inference**: Enter the main sampling loop, using CPU cycles directly to compute tensors and pipe the result to the Serial port.

---

### 2. Testing on Virtual Infrastructure (QEMU/VBox)
*   **Serial Debugging**: Use QEMU's `-serial stdio` redirection to capture kernel logs and trace tensor computation errors without needing a GUI.
*   **ISA Validation**: Test instruction sets by varying the emulated CPU (e.g., `-cpu host` vs `-cpu Westmere`) to ensure the runner respects hardware limits.
*   **Memory Pressure Tests**: Run the VM with the absolute minimum RAM required for the model to ensure the Page-Fault handler is robust.
*   **VirtualBox EFI Booting**: Validate that the runner boots correctly under VirtualBox's UEFI implementation to ensure broad PC compatibility.
*   **Deterministic Benchmarking**: Measure performance using CPU cycles (TSC) instead of wall-clock time to get a "pure" metric of inference speed.

**Algorithm: The Validation Loop**
1.  **Generate**: Compile the latest Kernel and bundle it with a small model (e.g., Qwen-0.5B).
2.  **Launch**: Execute `qemu-system-x86_64 -m 1G -drive file=qwen.pureblm,format=raw -serial stdio`.
3.  **Monitor**: Capture the serial stream; look for the "GGUF Magic" validation log.
4.  **Input**: Feed a test prompt into the virtual serial port.
5.  **Verify**: Compare the generated hex/text tokens against the known good output from a standard Linux run.

---

### 3. Ensuring Full "GGUF-to-Boot" Automation
*   **Unified Pure-Bundler**: Create a tool that takes a raw GGUF file and prepends the bootable kernel headers in one step.
*   **Checksum Integrity**: Embed a SHA-256 hash of the model data into the boot header to verify the model hasn't been corrupted on the storage media.
*   **Embedded Hyper-parameters**: Allow users to "bake in" default settings (context length, temperature, top-p) into the `.pureblm` file itself.
*   **Storage Driver Support**: Implement a simple IDE/SATA/NVMe driver so the AI can read models larger than the system RAM by streaming from the disk.
*   **BIOS-ISO Generation**: Automatically package the `.pureblm` into a `.iso` file so it can be burned to CD/USB by standard imaging tools like Rufus.

**Algorithm: The Pure-Bundler Pipeline**
1.  **Verify**: Open the input GGUF and validate its internal GGP (tensor) structure.
2.  **Header Patching**: Construct a 512-byte boot sector containing the "Stage 0" code and model metadata.
3.  **Alignment**: Pad the Kernel binary and Model data to 64KB boundaries to ensure hardware-friendly memory offsets.
4.  **Fusion**: Concatenate `[BootSector] + [Kernel] + [AlignmentPadding] + [ModelData] + [PureBLMFooter]`.
5.  **Finalization**: Generate a bootable `.iso` or `.img` and report the total system requirements (Min RAM) needed to run that specific bundle.
