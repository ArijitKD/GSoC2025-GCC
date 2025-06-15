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

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#undef errno
extern int errno;

#undef FILES_MAX
#undef FSIZE_MAX
#undef FNAME_MAX
#undef FOPEN_MAX

#define FILES_MAX  32		// Maximum number of files supported by the file system
#define FSIZE_MAX  4096		// Maximum supported file size
#define FNAME_MAX  32		// Maximum supported length of filename
#define FOPEN_MAX  5		// Maximum number of simultaneously open files 

#undef FILE_NOT_FOUND
#undef VRAMDISK_FULL

#define FILE_NOT_FOUND  -1
#define VRAMDISK_FULL   -1


// This is the actual file data structure with its metadata
struct File {
  ssize_t fid;			// File ID; maps to an index in vramdisk[]
  char fname[FNAME_MAX];	// Null-terminated string to store file name
  size_t fsize;			// Store file size in bytes
  uint8_t data[FSIZE_MAX];	// Actual file data
};

// This is a VRAM buffer simulating a disk to store all the files
// Indices of this "VRAM disk" is the file ID in the file system
// WARNING: This initialization is not standard C, but GCC supported
static struct File vramdisk[FILES_MAX] = {[0 ... FOPEN_MAX-1] = {.fid = -1}};;		

// This is the data structure that stores metadata about a currently open file
struct OpenFile {
  ssize_t fd;		// File descriptor; essentially the File ID
  size_t foffset;	// Current read/write offset within the file (0 <= offset <= fsize)
  uint8_t mode;		// The mode in which the file was opened
};

// The file table for all open files. All file IDs default to -1, indicating an empty entry in this file table.
static struct OpenFile open_files[FOPEN_MAX] = {[0 ... FOPEN_MAX-1] = {.fd = -1}};

// A variable to track the next open file index from open_files
static ssize_t next_open_file_index = 0;


/**************************************** INTERNAL SUBROUTINES ****************************************/

// Linearly searches for the file with filename == fname and returns its file ID, or FILE_NOT_FOUND
// on failure
static ssize_t get_file_id(const char *fname) {
  for (size_t i = 0; i < FILES_MAX; ++i) {
    if (vramdisk[i].fid != -1 && !strcmp(vramdisk[i].fname, fname))
      return vramdisk[i].fid;
  }
  return FILE_NOT_FOUND;
}

// Linearly scans through all entries in vramdisk[] and stops at the first entry with fid = -1, sets
// it to the index of the entry in vramdisk[] (i.e. the actual file ID) and returns the same. If all
// entries are exhausted, it means that no more available write space is left. Hence, VRAMDISK_FULL
// is returned in that case. The file name too is set using the fname parameter passed.
static ssize_t set_file_id(const char *fname) {
  for (size_t i = 0; i < FILES_MAX; ++i) {
    if (vramdisk[i].fid == -1) {
      vramdisk[i].fid = i;
      strncpy(vramdisk[i].fname, fname, FNAME_MAX);
      return vramdisk[i].fid;
    }
  }
  return VRAMDISK_FULL;
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

  vramdisk[0].fid = 0;
  strncpy(vramdisk[0].fname, "file.txt", FNAME_MAX);
  vramdisk[0].fsize = strlen("Hello world!");
  strncpy(vramdisk[0].data, "Hello world!", FSIZE_MAX);

  ssize_t i = next_open_file_index;
  
  if (i >= FOPEN_MAX)
    return -1;		// Max open files limit reached
  
  open_files[i].mode = flags;

  switch (flags) {

    case O_RDONLY:
      open_files[i].fd = get_file_id(pathname);
      if (open_files[i].fd == FILE_NOT_FOUND)
        return FILE_NOT_FOUND;
      open_files[i].foffset = 0;
      break;

    case O_WRONLY:
      open_files[i].fd = set_file_id(pathname);
      if (open_files[i].fd == VRAMDISK_FULL)
        return VRAMDISK_FULL;
      open_files[i].foffset = 0;
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

