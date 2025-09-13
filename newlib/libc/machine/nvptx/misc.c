/*
 * Support file for nvptx in newlib.
 * Copyright (c) 2014-2018 Mentor Graphics.
 * Copyright (c) 2025-Present Arijit Kumar Das <arijitkdgit.official@gmail.com>.
 *
 * The authors hereby grant permission to use, copy, modify, distribute,
 * and license this software and its documentation for any purpose, provided
 * that existing copyright notices are retained in all copies and that this
 * notice is included verbatim in any distributions. No written agreement,
 * license, or royalty fee is required for any of the authorized uses.
 * Modifications to this software may be copyrighted by their authors
 * and need not follow the licensing terms described here, provided that
 * the new terms are clearly indicated on the first page of each file where
 * they apply.
 */

#define __test__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#undef errno
extern int errno;

// Undefine all constants for safety
#undef MAX_FILES
#undef MAX_FNAME
#undef MAX_FOPEN

#undef MODE_R
#undef MODE_W
#undef MODE_A
#undef MODE_R_PLUS
#undef MODE_W_PLUS
#undef MODE_A_PLUS
#undef MODE_RW_TRUNC

#undef ERR_ENTRY_NOT_FOUND
#undef ERR_ENTRIES_EXHAUSTED
#undef ERR_NULLPTR
#undef ERR_NO_SPACE

#undef UNRESERVED_FD_START

#undef ENT_DEVNULL

#undef STDIN
#undef STDOUT
#undef STDERR


enum FileSystemLimits {
  MAX_FILES = 32,       // Maximum number of files supported
  MAX_FNAME = 32,		    // Maximum supported length of filename
  MAX_FOPEN = 8 		    // Maximum number of simultaneously open files
};


enum SupportedFileOpenModes {
  MODE_R = O_RDONLY,
  MODE_W = (O_WRONLY | O_CREAT | O_TRUNC),
  MODE_A = (O_WRONLY | O_CREAT | O_APPEND),
  MODE_R_PLUS = O_RDWR,
  MODE_W_PLUS = (O_RDWR | O_CREAT | O_TRUNC),
  MODE_A_PLUS = (O_RDWR | O_CREAT | O_APPEND),
  MODE_RW_TRUNC = (O_RDWR | O_TRUNC)
};


enum FileIOErrors {
  ERR_ENTRY_NOT_FOUND = -2,
  ERR_ENTRIES_EXHAUSTED = -3,
  ERR_NULLPTR = -4,
  ERR_NO_SPACE = -6
}; 


// This is the actual file system entry data structure with its metadata
struct Entry {
  char name[MAX_FNAME];    // Null-terminated string to store file name
  size_t size;             // Store file size in bytes
  char *data;              // Actual file data (dynamically allocated)
};


// Pre-define an entry for /dev/null.
#define ENT_DEVNULL {    \
  .name = "/dev/null",   \
  .size = 0,             \
  .data = NULL           \
}


// This is the data structure that stores metadata about a file
struct File {
  size_t offset;	          // Current read/write offset within the file (0 <= offset <= size)
  int mode;		              // The mode in which the file was opened
  struct Entry *entref;     // Reference to a file system entry
};


/* We don't need any buffering at all: reading from stdin just always returns
 * something like "no data", and write-ing to stdout, stderr just invokes printf.
 * Hence, entref for STDIN, STDOUT, STDERR as defined below are NULL.
 */
#define STDIN {                 \
  .offset = 0,                  \
  .mode = MODE_RW_TRUNC,        \
  .entref = NULL                \
}               
#define STDOUT {                \
  .offset = 0,                  \
  .mode = MODE_RW_TRUNC,        \
  .entref = NULL                \
}               
#define STDERR {                \
  .offset = 0,                  \
  .mode = MODE_RW_TRUNC,        \
  .entref = NULL                \
}


/* This is a VRAM buffer simulating a formatted disk to store all the entries
 * WARNING: This initialization is not standard C, but GCC supported
 */
static struct Entry vramfs[MAX_FILES] = {
  ENT_DEVNULL,
  [1 ... MAX_FILES - 1] = {
  .name = "",
  .size = 0,
  .data = NULL
}};


// File descriptors 0, 1 & 2 would be reserved for STDIN, STDOUT & STDERR respectively
#define UNRESERVED_FD_START 3


// The file table for all open files. STDIN, STDOUT, STDERR are open by default.
static struct File open_files[MAX_FOPEN] = {
  STDIN,      // fd = 0
  STDOUT,     // fd = 1
  STDERR,     // fd = 2
  [UNRESERVED_FD_START ... MAX_FOPEN - 1] = {
  .offset = 0,
  .mode = -1,
  .entref = NULL
}};


/**************************************** INTERNAL SUBROUTINES ****************************************/

/* IMPORTANT: PLEASE NOTE THAT WE USE strncpy() to copy the file name strings and memcpy() to copy the file
 * data. This is because we do not track the string lengths of file names externally, and so strncpy()'s
 * '/0' character termination helps. This nul character maybe a valid character in the file's data, and we
 * are dealing with raw bytes in such case. We track the file size externally using the size value of File. 
*/

#ifdef __test__
static void __test() {
  printf ("sizeof(_READ_WRITE_RETURN_TYPE) = %d bytes\n", sizeof(_READ_WRITE_RETURN_TYPE));
  printf ("sizeof(_ssize_t) = %d bytes\n", sizeof(_ssize_t));
  printf ("sizeof(ssize_t) = %d bytes\n", sizeof(ssize_t));

  const char *data = "Hello world!";
  strncpy(vramfs[UNRESERVED_FD_START].name, "hello_test.txt", MAX_FNAME);
  vramfs[UNRESERVED_FD_START].size = strlen(data);
  vramfs[UNRESERVED_FD_START].data = malloc(vramfs[UNRESERVED_FD_START].size + 1);
  if (vramfs[UNRESERVED_FD_START].data)
    memcpy(vramfs[UNRESERVED_FD_START].data, data, vramfs[UNRESERVED_FD_START].size);
}
#endif

static int find_entry(const char *name, struct Entry **entref_ptr) {
/* Searches for the entry with the given name in the file system.
 * TODO: Optimize searching algorithm
 */
  for (int i = 0; i < MAX_FILES; ++i) {
    if (!strcmp(vramfs[i].name, name)) {
      *entref_ptr = vramfs + i;
      return 0;
    }
  }
  return ERR_ENTRY_NOT_FOUND;
}

static int init_entry(const char *name, struct Entry **entref_ptr) {
/* Initializes an empty entry in the file system with the given name.
 * It is assumed that an entry with the given name doesn't exist in
 * the file system. Caller should verify this by running find_entry().
 * TODO: Optimize searching algorithm
 */
  for (int i = 0; i < MAX_FILES; ++i) {
    if (!strcmp(vramfs[i].name, "")) {
      strncpy(vramfs[i].name, name, MAX_FNAME);
      *entref_ptr = vramfs + i;
      return 0;
    }
  }
  return ERR_ENTRIES_EXHAUSTED;
}

static int clear_entry(struct Entry *entref) {
 /* Clears the data & metadata of the file system entry without removing it.
  * The name is left intact.
  */
  if (!entref)
    return ERR_NULLPTR;

  entref->size = 0;
  free(entref->data);
  entref->data = NULL;
  return 0;
}

static int read_entry_data(struct File *file, void *buf, size_t count, int *new_count_ref) {
/* Read the data from the file system entry that file's entref points to. Reading is started
 * from file's offset. Read data is copied into buf. On success, 0 is returned.
 */
  if ((!file) || (!file->entref))
    return ERR_NULLPTR;

  // If the data is NULL, don't modify buf & set *new_count_ref to 0 because there's no data (no error)
  if (!(file->entref)->data) {
    *new_count_ref = 0;
    return 0;
  }
  memcpy(buf, (file->entref)->data + file->offset, count);
  *new_count_ref = count;
  return 0;
}

static int write_entry_data(struct File *file, const void *buf, size_t count, int *new_count_ref) {
 /* Write the contents of buf to data of the file system entry that file's entref points to.
  * Writing is started from the file's offset. On success, 0 is returned.
  * *file should be a valid element of the open_files file table, otherwise KA-BOOM!!!
  */

  if ((!file) || (!file->entref))
    return ERR_NULLPTR;

  // If file is an element of open_files (which it should be), below should give the fd.
  int fd = (int)(file - open_files);

  char *cbuf = (char *)buf;

  // Handle the standard I/O files first
  switch (fd) {

    // STDIN (currently, it's not clear what writing to STDIN actually does, so below is a stub)
    case 0:
    *new_count_ref = count;
    return 0;

    // STDOUT (writing to STDOUT invokes printf)
    case 1:
    for (size_t i = 0; i < count; ++i)
      *new_count_ref += printf ("%c", cbuf[i]);
    return 0;

    // STDERR (writing to STDERR invokes printf)
    case 2:
    for (size_t i = 0; i < count; ++i)
      *new_count_ref += printf ("%c", cbuf[i]);
    return 0;
  }

  // For /dev/null
  if (!strcmp((file->entref)->name, "/dev/null")) {
    *new_count_ref = count;
    return 0;
  } 

  // For generic files
  size_t cur_size, new_size;
  cur_size = (file->entref)->size;
  new_size = cur_size + count;
    
  char *new_data = NULL;
  if (new_size > cur_size)
    new_data = realloc((file->entref)->data, new_size);

  if (!new_data)  // Probably out of memory
    return ERR_NO_SPACE;

  (file->entref)->data = new_data;
  memcpy((file->entref)->data + file->offset, buf, count);
  (file->entref)->size = new_size;
  *new_count_ref = count;

  return 0;
}
/*****************************************************************************************************/


/******************************************* SYSTEM CALLS *******************************************/
int
close(int fd) {

  // No illegal file descriptors allowed
  if (fd < 0 || fd > MAX_FOPEN - 1) {
    errno = EBADF;
    return -1;
  }

  // fd is a valid but not open file descriptor
  if (open_files[fd].mode == -1) {
    errno = EBADF;
    return -1;
  }

  if (fd < UNRESERVED_FD_START)
    return 0; // For all default open files (STDIN, STDOUT, STDERR)

  open_files[fd].offset = 0;
  open_files[fd].mode = -1;
  open_files[fd].entref = NULL;
  return 0;
}


int
fstat (int fd, struct stat *buf) {
  return -1;
}


int
gettimeofday (struct timeval *tv, void *tz) {
  return -1;
}


int
getpid (void) {
  return 0;
}


int
isatty (int fd) {
  return fd == 1;
}


int
kill (int pid, int sig) {
  errno = ESRCH;
  return -1;
}


off_t
lseek(int fd, off_t offset, int whence) {
  return 0;
}


int
open (const char *pathname, int flags, ...) {
  #ifdef __test__
  __test();
  #endif

  int fd;
  for (fd = UNRESERVED_FD_START; fd < MAX_FOPEN; ++fd) {

    // mode = -1 will always mean an empty slot in the file table of open_files
    if (open_files[fd].mode == -1)
      break;
  }

  if (fd == MAX_FOPEN) {
    errno = ENFILE;
    return -1;
  }

  struct Entry *entref;
  int errcode = find_entry(pathname, &entref);
  
  // Do not allow opening the file if the file exists and is open (set EACCES)
  if (errcode != ERR_ENTRY_NOT_FOUND) {
    int spal;
    for (spal = 0; spal < MAX_FOPEN; ++spal) {
      if (entref->name == (open_files[spal].entref)->name) {
        errno = EACCES;
        return -1;
      }
    }
  }

  switch (flags) {
    case MODE_R:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errno = ENOENT;
      return -1;
    }
    open_files[fd].offset = 0;
    open_files[fd].mode = MODE_R;
    open_files[fd].entref = entref;
    break;

    case MODE_W:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errcode = init_entry(pathname, &entref);
      if (errcode == ERR_ENTRIES_EXHAUSTED) {
        errno = ENOSPC;
        return -1;
      }
    }
    else {
      clear_entry(entref);
      /* We are not checking for the error code in
       * clear_entry() here because file
       * in this case is guaranteed to be valid.
       * This function returns only ERR_NULLPTR
       * if file is NULL, otherwise 0.
       */
    }
    open_files[fd].offset = 0;
    open_files[fd].mode = MODE_W;
    open_files[fd].entref = entref;
    break;

    case MODE_A:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errcode = init_entry(pathname, &entref);
      if (errcode == ERR_ENTRIES_EXHAUSTED) {
        errno = ENOSPC;
        return -1;
      }
    }
    open_files[fd].offset = entref->size;
    open_files[fd].mode = MODE_A;
    open_files[fd].entref = entref;
    break;

    case MODE_R_PLUS:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errno = ENOENT;
      return -1;
    }
    open_files[fd].offset = 0;
    open_files[fd].mode = MODE_R_PLUS;
    open_files[fd].entref = entref;
    break;

    case MODE_W_PLUS:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errcode = init_entry(pathname, &entref);
      if (errcode == ERR_ENTRIES_EXHAUSTED) {
        errno = ENOSPC;
        return -1;
      }
    }
    else {
      clear_entry(entref);
    }
    open_files[fd].offset = 0;
    open_files[fd].mode = MODE_W_PLUS;
    open_files[fd].entref = entref;
    break;

    case MODE_A_PLUS:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errcode = init_entry(pathname, &entref);
      if (errcode == ERR_ENTRIES_EXHAUSTED) {
        errno = ENOSPC;
        return -1;
      }
    }
    open_files[fd].offset = entref->size;
    open_files[fd].mode = MODE_A_PLUS;
    open_files[fd].entref = entref;
    break;

    case MODE_RW_TRUNC:
    if (errcode == ERR_ENTRY_NOT_FOUND) {
      errno = ENOENT;
      return -1;
    }
    clear_entry(entref);
    open_files[fd].offset = 0;
    open_files[fd].mode = MODE_RW_TRUNC;
    open_files[fd].entref = entref;
    break;

    default:
    errno = ENOTSUP;
    return -1;
  }

  return fd;
}

ssize_t
read(int fd, void *buf, size_t count) {

  // No illegal file descriptors allowed
  if (fd < 0 || fd > MAX_FOPEN - 1) {
    errno = EBADF;
    return -1;
  }

  struct File *file = open_files + fd;

  // Error if read attempt from a file opened with O_WRONLY
  if (file->mode == MODE_W || file->mode == MODE_A) {
    errno = EBADF;
    return -1;
  }
  
  int new_count;

  // fd is valid file descriptor so no need to check errcode
  read_entry_data(file, buf, count, &new_count);

  return new_count;
}

ssize_t
write (int fd, const void *buf, size_t count) {

  // No illegal file descriptors allowed
  if (fd < 0 || fd > MAX_FOPEN - 1) {
    errno = EBADF;
    return -1;
  }

  struct File *file = open_files + fd;

  // Error if write attempt to a file opened with O_RDONLY
  if (file->mode == MODE_R) {
    errno = EBADF;
    return -1;
  }

  int new_count;

  int errcode = write_entry_data(file, buf, count, &new_count);
  if (errcode == ERR_NO_SPACE) {
    errno = ENOSPC;
    return -1;
  }

  return count;
}

int
stat (const char *file, struct stat *pstat) {
  errno = EACCES;
  return -1;
}

void
sync (void) {
}

int
unlink (const char *pathname) {
  return -1;
}

/****************************************************************************************************/
