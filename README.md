## Google Summer of Code 2025 – GNU Compiler Collection

**Project:**  
_Implementation of a simple in-memory file system for running offloading tests on NVIDIA GPUs._

**Contributor:**  
Arijit Kumar Das (ArijitKD)

**Mentors:**  
Thomas Schwinge, Tobias Burnus


## Project Background
The GCC compiler supports a wide range of processor architectures for generating the corresponding binaries. This is not just limited to conventional CPUs, but also accelerator devices such as _Graphical Processing Units_ (GPUs), better known as graphics cards. The GCC compiler, built specifically for a given GPU architecture, is capable of generating an intermediate machine code which get Just-In-Time (JIT) compiled by the underlying GPU vendor-specific toolchains. Through a process called _Offloading_, it is possible to run a conventional GCC-compiled code on accelerator devices such as GPUs. The _OpenMP/OpenACC_ APIs implemented in GCC provide a backend for this purpose.

For GCC compilers with offloading support, the Newlib C library is used as the standard library when compiling code with offloading enabled. Newlib provides syscall stubs that need to be implemented for specific architectures. For running OpenMP/OpenACC offloading tests that require performing a file operation, syscalls such as `open()`, `read()`, `write()`, etc. need to be implemented. These syscalls would internally interface with a filesystem that would be running entirely in the GPU's own memory (the VRAM).


## Aim of the Project
This project aims to enable OpenMP/OpenACC file I/O-based offloading tests to run successfully on NVIDIA GPUs. This specifically requires modifying the stub syscalls in Newlib's NVPTX backend, which is the one used for NVIDIA GPUs.

The project is divided into two main parts:
- Develop an in-memory filesystem that runs entirely in the GPU's memory;
- Modify the file I/O-based stub syscalls to  interface with this filesystem.


## Project Design and Implementation

#### The Filesystem (`vramfs`)