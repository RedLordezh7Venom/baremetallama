# BareMetalLlama vs PureBLM Algorithms

## 1. .baremetallama Algorithm (OS-Dependent APE)

This diagram shows how the current `.baremetallama` format uses a polyglot header to run across Linux, Windows, and Mac.

```mermaid
sequenceDiagram
    participant User
    participant OS as Host OS (Win/Linux/Mac)
    participant BML as .baremetallama File
    participant Engine as APE AI Engine
    participant Model as GGUF Model Data

    User->>OS: Execute qwen.baremetallama
    OS->>BML: Read Header
    Note over BML: APE Header (MZ / #! / Polyglot)
    BML->>OS: Identify Platform Type
    Note right of OS: If Windows: Run as PE<br/>If Linux/Mac: Run as ELF/Mach-O
    OS->>Engine: Launch Engine (Offset 0)
    Engine->>Engine: Initialize (cosmopolitan libc)
    Engine->>BML: Read Footer (Last 20 bytes)
    BML-->>Engine: Return Model Offset & Size
    Engine->>Model: Memory Map (mmap) Model Data
    Note over Model: Direct Read from Self-Tail
    Engine->>User: Launch Chat TUI / API
```

---

## 2. .pureblm Algorithm (Bare-Metal RTOS)

This diagram shows the proposed bootable workflow where the AI runs directly on hardware without an OS.

```mermaid
sequenceDiagram
    participant Hardware as Hardware (CPU/RAM)
    participant Boot as Stage 0 Bootloader
    participant Kernel as Pure-Kernel (RTOS)
    participant Engine as Decoupled GGML Engine
    participant Model as GGUF Weights

    Hardware->>Boot: Power On / Legacy or UEFI Boot
    Boot->>Hardware: Memory Discovery (E820/EFI)
    Boot->>Kernel: Load Pure-Kernel into RAM
    Kernel->>Hardware: Setup GDT/IDT (Long Mode)
    Kernel->>Kernel: Initialize Serial/VGA Driver
    Kernel->>Model: Map Weights to Physical RAM
    Note over Model: Zero-Copy Identity Mapping
    Kernel->>Engine: Transfer Execution
    Engine->>Engine: CPU-Generic Tensor Init
    Engine->>Hardware: Multicore Parallelize (via IPI)
    Engine->>Hardware: Direct Write to Serial/Video Buffer
    Note right of Hardware: Constant "AI Intelligence" Loop
```
