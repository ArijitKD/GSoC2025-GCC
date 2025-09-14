## Google Summer of Code 2025 – GNU Compiler Collection

**Project:**  
_Implementation of a simple in-memory file system for running offloading tests on NVIDIA GPUs._

**Contributor:**  
Arijit Kumar Das (ArijitKD)

**Mentors:**  
Thomas Schwinge, Tobias Burnus


## Project Background
The GCC compiler supports a wide range of processor architectures for generating the corresponding binaries. This is not just limited to conventional CPUs, but also accelerator devices such as _Graphical Processing Units_ (GPUs), better known as graphics cards. The GCC compiler, built specifically for a given GPU architecture, is capable of generating an intermediate machine code which get Just-In-Time (JIT) compiled by the underlying GPU vendor-specific toolchains. Through a process called _Offloading_, it is possible to run a conventional GCC-compiled code on accelerator devices such as GPUs. The _OpenMP/OpenACC_ APIs implemented in GCC provide a backend for this purpose.
