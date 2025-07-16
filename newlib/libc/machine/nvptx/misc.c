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
  MAX_FILES = 32,       // Maximum number of files supported
  MAX_FSIZE = 4096,     // Maximum supported file size
  MAX_FNAME = 32,		    // Maximum supported length of filename
  MAX_FOPEN = 5			    // Maximum number of simultaneously open files 
};

enum SupportedFileOpenModes {
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
  ERR_INVALID_FILE_ID = -4,
  ERR_FILE_TOO_BIG = -5
};

// This is the actual file data structure with its metadata
struct File {
  int fid;                  // File ID; maps to an index in vramdisk[]
  char fname[MAX_FNAME];    // Null-terminated string to store file name
  size_t fsize;             // Store file size in bytes
  char data[MAX_FSIZE];     // Actual file data
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
  int fd;		    // File descriptor; essentially the File ID
  size_t offset;	// Current read/write offset within the file (0 <= offset <= fsize)
  int mode;		  // The mode in which the file was opened
};

// The file table for all open files. All file IDs default to -1, indicating an empty entry in this file table.
static struct OpenFile open_files[MAX_FOPEN] = {[0 ... MAX_FOPEN-1] = {
  .fd = -1,
  .offset = 0,
  .mode = -1
}};

// A variable to track the next open file index from open_files
static int next_open_file_index = 0;


/**************************************** INTERNAL SUBROUTINES ****************************************/

#ifdef __test__
static void __test() {
  printf ("sizeof(_READ_WRITE_RETURN_TYPE) = %d bytes\n", sizeof(_READ_WRITE_RETURN_TYPE));
  printf ("sizeof(_ssize_t) = %d bytes\n", sizeof(_ssize_t));
  printf ("sizeof(ssize_t) = %d bytes\n", sizeof(ssize_t));
  vramdisk[0].fid = 0;
  strncpy(vramdisk[0].fname, "file.txt", MAX_FNAME);
  vramdisk[0].fsize = strlen("Hello world!");
  strncpy(vramdisk[0].data, "Hello world!", MAX_FSIZE);
}
#endif

static int find_file(const char *fname, int *fid_addr) {
/* Searches for the file with name fname in the file system.
 * Linearly searches for the file with filename == fname, stores its file ID at fid_addr and returns 0
 * on success, or ERR_FILE_NOT_FOUND on failure.
 *
 * TODO: Optimize searching algorithm
 */
  for (int i = 0; i < MAX_FILES; ++i) {
    if (vramdisk[i].fid != -1 && !strcmp(vramdisk[i].fname, fname)) {
      *fid_addr = vramdisk[i].fid;
      return 0;
    }
  }
  return ERR_FILE_NOT_FOUND;
}

static int create_file(const char *fname, int *fid_addr) {
/* Creates a new empty file with file name as fname.
 * Linearly scans through all entries in vramdisk[] and stops at the first entry with fid = -1, sets
 * it to the index of the entry in vramdisk[] (i.e. the actual file ID), stores the same at fid_addr,
 * and returns 0 on success. If all entries are exhausted, it means that no more available write space
 * is left. Hence, ERR_VRAMDISK_FULL is returned in that case. The file name too is set using the fname
 * parameter passed. The file size and file data are not touched, assuming that they're already clear,
 * i.e. either uninitialized or deleted.
 *
 * TODO: Optimize searching algorithm
 */
  for (int i = 0; i < MAX_FILES; ++i) {
    if (vramdisk[i].fid == -1) {
      vramdisk[i].fid = i;
      strncpy(vramdisk[i].fname, fname, MAX_FNAME);
      *fid_addr = vramdisk[i].fid;
      return 0;
    }
  }
  return ERR_VRAMDISK_FULL;
}

static int get_fsize_from_fid(int fid, size_t *fsize_addr) {
/* Get the file size from the file ID as in vramdisk[fid], store it at fsize_addr, and return
 * 0 on success. If the fid passed is out of bounds, returns ERR_INVALID_FILE_ID. If the fid
 * value read from vramdisk[fid] is -1, it implies the file does not exist for the given fid
 * value, hence ERR_FILE_NOT_FOUND is returned.
 */
  if (fid >= 0 && fid < MAX_FILES) {
    if (vramdisk[fid].fid != -1) {
      *fsize_addr = vramdisk[fid].fsize;
      return 0;
    }
    else {
      return ERR_FILE_NOT_FOUND;
    }
  }
  return ERR_INVALID_FILE_ID;
}

static int set_fsize_from_fid(int fid, size_t fsize) {
/* Sets the file size from the file ID as in vramdisk[fid]. If fsize exceeds MAX_FSIZE, returns
 * ERR_FILE_TOO_BIG. If the fid passed is out of bounds, returns ERR_INVALID_FILE_ID. If the fid
 * value read from vramdisk[fid] is -1, it implies the file does not exist for the given fid
 * value, hence ERR_FILE_NOT_FOUND is returned.
 */
  if (fsize > MAX_FSIZE)
    return ERR_FILE_TOO_BIG;

  if (fid >= 0 && fid < MAX_FILES) {
    if (vramdisk[fid].fid != -1) {
      vramdisk[fid].fsize = fsize;
      return 0;
    }
    else {
      return ERR_FILE_NOT_FOUND;
    }
  }
  return ERR_INVALID_FILE_ID;
}

static int truncate_file_from_fid(int fid) {
 /* Truncate the contents of a file with the given file ID (fid). Essentially, clears the data
  * buffer for the File struct with given fid and returns 0 on success. If the file ID passed
  * is invalid, returns ERR_INVALID_FILE_ID.
  */
  if (fid >= 0 && fid < MAX_FILES) {
    memset(vramdisk[fid].data, 0, vramdisk[fid].fsize);
    vramdisk[fid].fsize = 0;
    return 0;
  }
  return ERR_INVALID_FILE_ID;
}

static int write_to_file_from_fid(int fid, const void *buf, size_t count, size_t offset) {
/* Write data to a file with given file ID from given offset from buf. On success, returns 0.
 * Returns ERR_INVALID_FILE_ID if the file ID is out of bounds. ASSUMES THAT THE FILE EXISTS
 * (i.e. vramdisk[fid].fid is NOT UPDATED. Also, will return ERR_FILE_TOO_BIG without any
 * file modification if new file size after write exceeds MAX_FSIZE. Call this function only
 * if fid passed is the file ID of an open file (fid == open_files[i].fd).
 */
  if (fid >= 0 && fid < MAX_FILES) {
    size_t cur_fsize;
    get_fsize_from_fid(fid, &cur_fsize);  // fid guaranteed to be valid, thus no errcode check
    size_t new_fsize = cur_fsize + count;
    
    if (new_fsize > MAX_FSIZE)
      return ERR_FILE_TOO_BIG;

    memcpy(vramdisk[fid].data + offset, buf, count);
    set_fsize_from_fid(fid, new_fsize);
    return 0;
  }
  return ERR_INVALID_FILE_ID;
}

static int read_file_from_fid(int fid, void *buf, size_t count, size_t offset, int *new_count_addr) {
/* Read data from a file with given file ID from given offset into buf. On success, returns 0.
 * Returns ERR_INVALID_FILE_ID if the file ID is out of bounds. ASSUMES THAT THE FILE EXISTS.
 * Call this function only if fid passed is the file ID of an open file (fid == open_files[i].fd).
 */
  if (fid >= 0 && fid < MAX_FILES) {
    size_t fsize;
    get_fsize_from_fid(fid, &fsize);  // fid guaranteed to be valid, thus no errcode check
    size_t bytes_to_write = count - offset;
    *new_count_addr = bytes_to_write > 0 ? bytes_to_write : count;

    memcpy(buf, vramdisk[fid].data + offset, *new_count_addr);
    return 0;
  }
  return ERR_INVALID_FILE_ID;
}
/*****************************************************************************************************/


/******************************************* SYSTEM CALLS *******************************************/

int
close(int fd) {
  int spal;
  for (spal = 0; spal < MAX_FOPEN; ++spal) {
    if (fd == open_files[spal].fd)
      break;
  }
  if (spal == MAX_FOPEN) {
    errno = EBADF;
    return -1;
  }

  for (spal = fd; open_files[spal].fd != -1 || spal < MAX_FOPEN - 1; ++spal) {
    open_files[spal].fd = open_files[spal + 1].fd;
    open_files[spal].offset = open_files[spal + 1].offset;
    open_files[spal].mode = open_files[spal + 1].mode;
  }
  open_files[spal].fd = -1;
  open_files[spal].offset = 0;
  open_files[spal].mode = -1;
  
  next_open_file_index--;
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
  int i = next_open_file_index;
  int fid = -1;
  size_t fsize;

  if (i == MAX_FOPEN) {
    errno = ENFILE;
    return -1;
  }

  int errcode = 0;
  errcode = find_file(pathname, &fid);
  
  if (errcode != ERR_FILE_NOT_FOUND) {
    int spal;
    for (spal = 0; spal < MAX_FOPEN; ++spal) {
      if (fid == open_files[spal].fd) {
        errno = EACCES;
        return -1;
      }
    }
  }

  switch (flags) {
    case MODE_R:
    if (errcode == ERR_FILE_NOT_FOUND) {
      errno = ENOENT;
      return -1;
    }
    open_files[i].fd = fid;
    open_files[i].offset = 0;
    open_files[i].mode = MODE_R;
    break;

    case MODE_W:
    if (errcode == ERR_FILE_NOT_FOUND) {
      errcode = create_file(pathname, &fid);
      if (errcode == ERR_VRAMDISK_FULL) {
        errno = ENOSPC;
        return -1;
      }
    }
    else {
      truncate_file_from_fid(fid);
      /* We are not checking for the error code in
       * truncate_file_from_fid() here because fid
       * in this case is guaranteed to be valid.
       * This function returns only ERR_INVALID_FILE_ID
       * if fid is invalid, otherwise 0.
       */
    }
    open_files[i].fd = fid;
    open_files[i].offset = 0;
    open_files[i].mode = MODE_W;
    break;

    case MODE_A:
    if (errcode == ERR_FILE_NOT_FOUND) {
      errcode = create_file(pathname, &fid);
      if (errcode == ERR_VRAMDISK_FULL) {
        errno = ENOSPC;
        return -1;
      }
    }
    open_files[i].fd = fid;
    get_fsize_from_fid(fid, &fsize);
    /* get_fsize_from_fid() returns only two types of errors:
     * ERR_FILE_NOT_FOUND and ERR_INVALID_FILE_ID. We are not
     * checking for the error codes here because fid in this
     * case is guaranteed to be valid, and only valid when the
     * file exists. Otherwise, open() aborts after setting the
     * appropriate errno.
     */

    if (fsize == MAX_FSIZE) {
      /* File has reached max file size limit and can no longer
       * be opened with O_APPEND flag. Therefore, EFBIG is set.
       */
      errno = EFBIG;
      return -1;
    }
    open_files[i].offset = fsize;
    open_files[i].mode = MODE_A;
    break;

    case MODE_R_PLUS:
    if (errcode == ERR_FILE_NOT_FOUND) {
      errno = ENOENT;
      return -1;
    }
    open_files[i].fd = fid;
    open_files[i].offset = 0;
    open_files[i].mode = MODE_R_PLUS;
    break;

    case MODE_W_PLUS:
    
    if (errcode == ERR_FILE_NOT_FOUND) {
      errcode = create_file(pathname, &fid);
      if (errcode == ERR_VRAMDISK_FULL) {
        errno = ENOSPC;
        return -1;
      }
    }
    else {
      truncate_file_from_fid(fid);
    }
    open_files[i].fd = fid;
    open_files[i].offset = 0;
    open_files[i].mode = MODE_W_PLUS;
    break;

    case MODE_A_PLUS:
    if (errcode == ERR_FILE_NOT_FOUND) {
      errcode = create_file(pathname, &fid);
      if (errcode == ERR_VRAMDISK_FULL) {
        errno = ENOSPC;
        return -1;
      }
    }
    open_files[i].fd = fid;
    get_fsize_from_fid(fid, &fsize);

    if (fsize == MAX_FSIZE) {
      errno = EFBIG;
      return -1;
    }
    open_files[i].offset = fsize;
    open_files[i].mode = MODE_A_PLUS;
    break;

    default:
    errno = ENOTSUP;
    return -1;
  }

  next_open_file_index++;
  return open_files[i].fd;
}

_READ_WRITE_RETURN_TYPE
read(int fd, void *buf, size_t count) {

  // Why are you passing illegal file descriptors bruh? :-(
  if (fd < 0 || fd > MAX_FILES - 1) {
    errno = EINVAL;
    return -1;
  }

  int i;
  for (i = 0; i < MAX_FOPEN; ++i) {
    if (open_files[i].fd == fd)
      break;
  }

  // Requested read from a file that's not open
  if (i == MAX_FOPEN)
    return 0;

  struct OpenFile *file = &open_files[i];

  // Error if read attempt from a file opened with O_WRONLY
  if (file->mode == MODE_W || file->mode == MODE_A) {
    errno = EBADF;
    return -1;
  }
  
  int new_count;

  // fd is valid file descriptor so no need to check errcode
  read_file_from_fid(fd, buf, count, file->offset, &new_count);

  return new_count;
}

_READ_WRITE_RETURN_TYPE
write (int fd, const void *buf, size_t count) {

  // Why are you passing illegal file descriptors bruh? :-(
  if (fd < 0 || fd > MAX_FILES - 1) {
    errno = EINVAL;
    return -1;
  }

  int i;
  for (i = 0; i < MAX_FOPEN; ++i) {
    if (open_files[i].fd == fd)
      break;
  }

  // Requested write to a file that's not open
  if (i == MAX_FOPEN)
    return 0;

  struct OpenFile *file = &open_files[i];

  // Error if write attempt to a file opened with O_RDONLY
  if (file->mode == MODE_R) {
    errno = EBADF;
    return -1;
  }

  int errcode = write_to_file_from_fid(fd, buf, count, file->offset);
  if (errcode == ERR_FILE_TOO_BIG) {
    errno = EFBIG;
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

