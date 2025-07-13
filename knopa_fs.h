#ifndef _KNOPA_FS_H
#define _KNOPA_FS_H

#include "kmem.h"
#include <c_types.h>
#include <spi_flash.h>

#define MAX_FILENAME_LEN  32
#define MAX_MIMETYPE_LEN  128

typedef struct {                   // 174 bytes
  char name[MAX_FILENAME_LEN+1];   // +zero terminator
  char mime[MAX_MIMETYPE_LEN+1];   // +zero terminator
  uint32 datetime;                 // UNIX
  uint32 size;                     // file size
  uint32 pos;                      // runtime
} t_File; 

typedef struct {
  char name[32];
  uint16 pos;
  uint16 len;
  bool (*Next)(void);
  t_File file;
} t_Dir;

typedef enum {
  KFS_SEEK_SET,
  KFS_SEEK_CUR,
  KFS_SEEK_HEAD,
  KFS_SEEK_END
} e_SeekOrigin;

typedef struct {
  uint32 start_segment;
  uint32 length_segments;
  uint32 total;
  uint32 used;
  uint32 data;
  uint16 nfiles;
} t_Stats;

// ============================================================================
// Set KFS params, check FS integrity and perform relevant actions as needed
//
// In:  start_seg  : first KFS file system segment (in flash memory, so segment 0 is usually boot segment)
//      length_seg : length of file system, in segments. Note that first 3 are reserved for FAT1, FAT2 and TEMP.
//
// Out: bool       : correct KFS detected at specified location
//
bool    FLASH_CODE KFS_Init ( uint32 start_seg, uint32 length_seg );

// ============================================================================
// Create KFS file system at specified location
//
// In:  start_seg  : first KFS file system segment (in flash memory, so segment 0 is usually boot segment)
//      length_seg : length of file system, in segments. Note that first 3 are reserved for FAT1, FAT2 and TEMP.
//
// Out: bool       : success / fail for some reason (see KFS_Error/KFS_ErrorMessage)
//
bool    FLASH_CODE KFS_Format ( uint32 start_seg, uint32 length_seg );

// ============================================================================
// Get file system statistics
//
// Out: t_Stats    : file system info (see above)
//
t_Stats* FLASH_CODE KFS_Stats ( void );

// ============================================================================
// Open directory
//
// Out: dir struct (see above)
//
t_Dir*  FLASH_CODE KFS_OpenDir ( char* name );

// ============================================================================
// Close directory
//
void FLASH_CODE KFS_CloseDir (void);

// ============================================================================
// Next file in directory
//
// Out: false = no more files to list
//      true  = dir struct filled up with next file's info
//
static bool FLASH_CODE KFS_DirNext ( void );

// ============================================================================
// Check if file exists
//
// Out: true/false
//
bool    FLASH_CODE KFS_FileExists ( char* name );

// ============================================================================
// Create/open file in KFS
//
// In:  name : file name
//      mode : r, w, a, r+, w+, a+
//      mime : mime type (for w, a, w+, a+)
//      size : file size (for w, a, w+) :: if set to 0 then file is expandable until closed.
// Modes:
//      r   (1) : Reading. File must exist. Seek to 0.
//      w   (2) : Writing. File is created if not exists, overwritten if exists. Seek to 0.
//      a   (3) : Append.  File is created if not exists. Seek to EOF.
//      r+  (4) : Update both reading & writing. File must exist. Seek to 0.
//      w+  (5) : Create for reading & writing. Created if not exists, overwritten if exists. Seek to 0.
//      a+  (6) : Open for reading & appending. File must exist. Seek to EOF.
//
// Out: t_File* / NULL
//
t_File* FLASH_CODE KFS_Open ( char* name, char* mode, char* mime, uint32 size );

// ============================================================================
// Close file
//
// Out: bool    : true = OK, false = error
//
bool    FLASH_CODE KFS_Close ( t_File *f );

// ============================================================================
// Rename file
//
// Out: true / false = success / error
//
bool    FLASH_CODE KFS_Rename (char* oldname, char* newname);

// ============================================================================
// Seek file
//
//   Move read/write position within a file
//
// In:  f      : file handle
//      offset : number of bytes to offset from {origin}
//      origin : position used as a reference for the {offset}
//
// Out: new file position
//
uint32  FLASH_CODE KFS_Seek ( t_File *f, sint32 offset, e_SeekOrigin origin );

// ============================================================================
// Read/write file
//
//   Read file from current position {fn->pos} into {buf} and move position {len} bytes ahead.
//   Write file starting from current position {fn->pos} from {buf} and move position {len} bytes ahead.
//
//  In: f    : file handle
//      buf  : read buffer
//      len  : number of bytes to read/write
//      rw   : 0 = read, 1 = write
//
// Out:  0+  : number of bytes actually read/written
//
sint32  FLASH_CODE KFS_Read ( t_File *f, char *buf, uint32 len );
sint32  FLASH_CODE KFS_Write ( t_File *f, char *buf, uint32 len );

// ============================================================================
// Delete file and shrink FS if needed
//
// In:  name : file name (with full path)
//
// Out: true = ok, false = error (see KFS_Error for more info)
//
bool    FLASH_CODE KFS_Delete ( char* name );

// ============================================================================
// Read most recent FS error code
//
uint8   FLASH_CODE KFS_Error ( void );

// ============================================================================
// Get most recent FS error message
//
char*   FLASH_CODE KFS_ErrorMessage ( void );

// ============================================================================
// Get file extension (without dot)
//
char*   FLASH_CODE KFS_FileExtension ( char* );

#endif
