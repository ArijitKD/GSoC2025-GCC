## Google Summer of Code 2025 – GNU Compiler Collection

**Project:**  
_Implementation of a simple in-memory file system for running offloading tests on NVIDIA GPUs._

**Contributor:**  
[Arijit Kumar Das](https://www.linkedin.com/in/arijitkd)

**Mentors:**  
[Thomas Schwinge](https://www.linkedin.com/in/tschwinge)  
[Tobias Burnus](https://www.linkedin.com/in/tobias-burnus-0b886a7)

---

## Project Background
The GCC compiler supports a wide range of processor architectures for generating the corresponding binaries. This is not just limited to conventional CPUs, but also accelerator devices such as _Graphical Processing Units_ (GPUs), better known as graphics cards. The GCC compiler, built specifically for a given GPU architecture, is capable of generating an intermediate machine code which gets Just-In-Time (JIT) compiled by the underlying GPU vendor-specific toolchains. Through a process called _Offloading_, it is possible to run a conventional GCC-compiled code on accelerator devices such as GPUs. The _OpenMP/OpenACC_ APIs implemented in GCC provide a backend for this purpose.

For GCC compilers with offloading support, the Newlib C library is used as the standard library when compiling code with offloading enabled. Newlib provides syscall stubs that need to be implemented for specific architectures. For running OpenMP/OpenACC offloading tests that require performing a file operation, syscalls such as `open()`, `read()`, `write()`, etc. need to be implemented and interact with a filesystem driver.

---

## Aim of the Project
This project aims to enable OpenMP/OpenACC file I/O-based offloading tests to run successfully on NVIDIA GPUs. This specifically requires modifying the stub syscalls in Newlib's NVPTX backend, which is the one used for NVIDIA GPUs.

The project is divided into two main parts:
- Develop an in-memory filesystem that runs entirely in the GPU's memory;
- Modify the file I/O-based stub syscalls to  interface with this filesystem.

---

## Design and Implementation

### The Filesystem
The filesystem is very simple by design. It is a statically allocated buffer (called `vramfs`) made up of individual units, singularly called an **Entry**. An **Entry** is a data structure which stores a file's name, size and data. It looks like this:
```
struct Entry {
    char name[MAX_FNAME];
    size_t size;
    char *data;
}
```
Each Entry in `vramfs` is initialized to `{.name = "", .size = 0, .data = NULL}` which represents an empty slot. Additionally, a special entry `{.name = "/dev/null", .size = 0, .data = NULL}` has been implemented to simulate the null device on POSIX systems. 

A second statically allocated buffer (called `open_files`) keeps track of all the files that are currently open. In the context of this filesystem, a **File** is a data structure which holds information about the current read/write offset in an Entry's data, the opening mode, and a reference to the Entry that is being operated upon. It looks like this:
```
struct File {
    size_t offset;
    int mode;
    struct Entry *entref;
}
```
Each File in `open_files` is initialized to `{.offset = 0, .mode = -1, .entref = NULL}`, representing an empty slot. The index of a non-empty slot in `open_files` gives the file descriptor associated with the corresponding File. File descriptors 0, 1, 2 are associated with STDIN, STDOUT, and STDERR as per requirements and pre-initialized accordingly. As such, only file descriptors 3 or higher (upto `MAX_FOPEN - 1`) are available for use.

### Directories
Directories are currently not supported, and was out of scope for this project. However, if a requirement arises, they may be implemented in the future.

### Locking for open files
Files that are open are currently 'locked' to prevent unwanted modifications. Calling `open()` for an already open file sets `errno` to `EACCES`.

### Supported file open modes
Only a few file open modes have been implemented, keeping in mind the simple usage that the filesystem is intended for. These are:
- `MODE_R` = `O_RDONLY`, equivalent to the `"r"` mode
- `MODE_W` = `(O_WRONLY | O_CREAT | O_TRUNC)`, equivalent to the `"w"` mode
- `MODE_A` = `(O_WRONLY | O_CREAT | O_APPEND)`, equivalent to the `"a"` mode
- `MODE_R_PLUS` = `O_RDWR`, equivalent to the `"r+"` mode
- `MODE_W_PLUS` = `(O_RDWR | O_CREAT | O_TRUNC)`, equivalent to the `"w+"` mode
- `MODE_A_PLUS` = `(O_RDWR | O_CREAT | O_APPEND)`, equivalent to the `"a+"` mode
- `MODE_RW_TRUNC` = `(O_RDWR | O_TRUNC)`, special mode for handling STDIO files

These should be enough to handle most use cases. In case an attempt is made to open a file using some other flag combination, `open()` sets `errno` to `ENOTSUP`.

### Filesystem limits
Since we have statically allocated buffers, we have constants for imposing the buffer size. These are:
- `MAX_FILES = 32`: Number of Entries that the filesystem can handle (the `vramfs` buffer)
- `MAX_FNAME = 32`: Maximum supported file name length, including the terminating `'\0'` character.
- `MAX_FOPEN = 8`: Maximum number of files that can be open simultaneously (Inclusive of STDIN, STDOUT and STDERR).

### Syscalls
As of **15 September 2025**, the following syscalls have been implemented:
- `open()`
- `read()`
- `write()`
- `close()`

These are located in the source files under `<newlib-repo-root>/newlib/libc/machine/nvptx`, especially in `misc.c`.

Other necessary syscalls are expected to be implemented soon.

### POSIX `errno`s used
- `EBADF`: Used in syscalls accepting a file descriptor (`fd`) to indicate an illegal `fd` value.
- `ENOSPC`: Used in `write()` and indicates a failure in allocating space for the requested write data, possibly due to memory shortage.
- `EFAULT`: Used in syscalls that receive pointers, indicates a NULL pointer exception.
- `ENFILE`: Used in `open()`, indicates that the maximum number of open files has been reached.
- `ENOENT`: Used in `open()`, indicates that the requested Entry was not found in the filesystem.
- `ENOTSUP`: Used in `open()`, indicates that an unsupported file open mode has been passed.
- `EACCES`: Used in `open()`, indicates that an attempt has been made to open an already opened file.

---

## Merged commits
The filesystem related code is still under review, as of **15 September 2025**. However, [this commit](https://sourceware.org/git/?p=newlib-cygwin.git;a=commit;h=5d8c71af5e0fa5cdc99d9f741624920e34756418) has been merged into Newlib.

---

## Conclusion
This 14 weeks of GSoC was a productive period for me. I have always wanted to work on real-world systems programming projects, but didn't find a helpful way to start. This was a great learning experience for me, and I believe I have learnt more C in the past few months than in the last half-decade of my experience with C programming.

I sincerely thank my mentors for providing constructive feedback on my work and guiding me when required. There's still a lot of work that needs to be done, and I hope to collaborate with my mentors and stay connected to the GCC project post-GSoC.
