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
#undef MAX_FSIZE
#undef MAX_FNAME
#undef MAX_FOPEN
#undef MODE_R
#undef MODE_W
#undef MODE_A
#undef MODE_R_PLUS
#undef MODE_W_PLUS
#undef MODE_A_PLUS
#undef ERR_FILE_NOT_FOUND
#undef ERR_VRAMDISK_FULL

enum FileSystemLimits {
  MAX_FILES = 32,		// Maximum number of files supported
  MAX_FSIZE = 4096,		// Maximum supported file size
  MAX_FNAME = 32,		// Maximum supported length of filename
  MAX_FOPEN = 5			// Maximum number of simultaneously open files 
};

enum FileOpenModes {
  MODE_R = O_RDONLY,
  MODE_W = (O_WRONLY | O_CREAT | O_TRUNC),
  MODE_A = (O_WRONLY | O_CREAT | O_APPEND),
  MODE_R_PLUS = O_RDWR,
  MODE_W_PLUS = (O_RDWR | O_CREAT | O_TRUNC),
  MODE_A_PLUS = (O_RDWR | O_CREAT | O_APPEND)
};

enum FileIOErrors {
  ERR_FILE_NOT_FOUND = -2,
  ERR_VRAMDISK_FULL = -3,
  ERR_INVALID_FILE_ID = -4
};

// This is the actual file data structure with its metadata
struct File {
  ssize_t fid;			// File ID; maps to an index in vramdisk[]
  char fname[MAX_FNAME];	// Null-terminated string to store file name
  size_t fsize;			// Store file size in bytes
  uint8_t data[MAX_FSIZE];	// Actual file data
};

// This is a VRAM buffer simulating a disk to store all the files
// Indices of this "VRAM disk" is the file ID in the file system
// WARNING: This initialization is not standard C, but GCC supported
static struct File vramdisk[MAX_FILES] = {[0 ... MAX_FILES-1] = {
	.fid = -1,
	.fname = "",
	.fsize = 0,
	.data = ""
}};

// This is the data structure that stores metadata about a currently open file
struct OpenFile {
  ssize_t fd;		// File descriptor; essentially the File ID
  off_t foffset;	// Current read/write offset within the file (0 <= offset <= fsize)
  int mode;		// The mode in which the file was opened
};

// The file table for all open files. All file IDs default to -1, indicating an empty entry in this file table.
static struct OpenFile open_files[MAX_FOPEN] = {[0 ... MAX_FOPEN-1] = {
	.fd = -1,
	.foffset = 0,
	.mode = -1
}};

// A variable to track the next open file index from open_files
static ssize_t next_open_file_index = 0;


/**************************************** INTERNAL SUBROUTINES ****************************************/

static ssize_t find_file(const char *fname) {
/* Linearly searches for the file with filename == fname and returns its file ID, or ERR_FILE_NOT_FOUND
 * on failure
 *
 * TODO: Optimize searching algorithm
 */
  for (size_t i = 0; i < MAX_FILES; ++i) {
    if (vramdisk[i].fid != -1 && !strcmp(vramdisk[i].fname, fname))
      return vramdisk[i].fid;
  }
  return ERR_FILE_NOT_FOUND;
}

static ssize_t create_file(const char *fname) {
/* Linearly scans through all entries in vramdisk[] and stops at the first entry with fid = -1, sets
 * it to the index of the entry in vramdisk[] (i.e. the actual file ID) and returns the same. If all
 * entries are exhausted, it means that no more available write space is left. Hence, ERR_VRAMDISK_FULL
 * is returned in that case. The file name too is set using the fname parameter passed. The file size
 * and file data are not touched.
 *
 * TODO: Optimize searching algorithm
 */
  for (size_t i = 0; i < MAX_FILES; ++i) {
    if (vramdisk[i].fid == -1) {
      vramdisk[i].fid = i;
      strncpy(vramdisk[i].fname, fname, MAX_FNAME);
      return vramdisk[i].fid;
    }
  }
  return ERR_VRAMDISK_FULL;
}

static ssize_t get_fsize_from_fid(ssize_t fid) {
/* Get the file size from the file ID as in vramdisk[fid]. If the fid passed is negative, returns
 * ERR_INVALID_FILE_ID. If the fid value read from vramdisk[fid] is -1, it implies the file does
 * not exist for the given fid value, hence ERR_FILE_NOT_FOUND is returned.
 */
  if (fid >= 0 && fid < MAX_FILES) {
    if (vramdisk[fid].fid != -1)
      return vramdisk[fid].fsize;
    else
      return ERR_FILE_NOT_FOUND;
  }
  return ERR_INVALID_FILE_ID;
}

/*****************************************************************************************************/


/******************************************* SYSTEM CALLS *******************************************/

int
close(int fd) {
  return -1;
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
  // Below initialization is just for test
  vramdisk[0].fid = 0;
  strncpy(vramdisk[0].fname, "file.txt", MAX_FNAME);
  vramdisk[0].fsize = strlen("Hello world!");
  strncpy(vramdisk[0].data, "Hello world!", MAX_FSIZE);

  ssize_t i = next_open_file_index;
  ssize_t fid = -1;

  if (i >= MAX_FOPEN) {
    errno = ENFILE;	// Max open files limit reached
    return -1;
  }

  switch (flags) {

    case MODE_R:
      fid = find_file(pathname);
      if (fid == ERR_FILE_NOT_FOUND) {
        errno = ENOENT;		// File does not exist
        return -1;
      }
      open_files[i].fd = fid;
      open_files[i].foffset = 0;
      open_files[i].mode = MODE_R;
      break;

    case MODE_W:
      fid = create_file(pathname);
      if (fid == ERR_VRAMDISK_FULL) {
        errno = ENOSPC;		// File system memory is full
        return -1;
      }
      open_files[i].fd = fid;
      open_files[i].foffset = 0;
      open_files[i].mode = MODE_W;
      break;
    
    case MODE_A:
      fid = create_file(pathname);
      if (fid == ERR_VRAMDISK_FULL) {
        errno = ENOSPC;		// File system memory is full
        return -1;
      }
      open_files[i].fd = fid;
      open_files[i].foffset = get_fsize_from_fid(fid);
      open_files[i].mode = MODE_A;
      break;
  }

  next_open_file_index++;
  return open_files[i].fd;
}

int
read(int fd, void *buf, size_t count) {
  return 0;
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

