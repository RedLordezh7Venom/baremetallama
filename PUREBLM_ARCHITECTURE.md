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

1. **Implementing it**: Develop the freestanding micro-kernel and the OS-agnostic GGML backend.
2. **Testing it on Virtual Machines**: Verify boot success and inference stability using **VirtualBox** and **QEMU** (emulating x86_64/ARM64 hardware).
3. **Ensuring it makes everything and runs**: Finalize the `pure-bundler` tool to ensure a seamless "GGUF-to-ISO" conversion that results in a fully functional bootable runner.
