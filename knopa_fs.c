#include <osapi.h>
//#include <mem.h>
#include <os_type.h> // os_timer_t

#include "knopa_fs.h"
//#include "knopa_time.h"
#include "knopa_misc.h"
#include "knopa_sys.h"

#define SEGMENT_SIZE  SPI_FLASH_SEC_SIZE

#define TEMP_SEGMENT 2            // temp segment number
#define SYS_SEGMENTS 3            // system segments (2xFAT + temp)

#define MAX_FILES 85              // max number of files in FAT

#define MAX_OPEN_FILES 6          // max number of open files

#define MIME_SIZE 64              // max MIME type length -- on flash

#define CTRL_SIZE 8               // control area length
#define CRC_SIZE  4               // CRC field in control area
#define DATA_SIZE (SEGMENT_SIZE-CTRL_SIZE) // segment data area length

#define _MIN(a,b) ((a)<(b)?(a):(b))
#define _MAX(a,b) ((a)>(b)?(a):(b))
#define _ABS(i) ((i)<0 ? -(i) : (i))

#define EXT_FILE  0
#define EXT_NEXT  1

// -------------------------------------------------------

#define F_ERR_OK                  0
#define F_ERR_FORMAT              1
#define F_ERR_INIT                2
#define F_ERR_FS_LENGTH           3
#define F_ERR_FLASH_ERASE         4
#define F_ERR_FLASH_READ          5
#define F_ERR_FLASH_WRITE         6
#define F_ERR_CRC                 7
#define F_ERR_FAT_ALLOC           8
#define F_ERR_FAT_EMPTY           9
#define F_ERR_TOO_MANY_OPEN      10
#define F_ERR_BAD_OPEN_MODE      11
#define F_ERR_FILE_NOT_FOUND     12
#define F_ERR_ALREADY_OPEN       13
#define F_ERR_EXP_NO_RESIZE      14
#define F_ERR_EXP_NO_CREATE      15
#define F_ERR_FAT_IS_FULL        16
#define F_ERR_FILE_ALLOC         17
#define F_ERR_BAD_HANDLE         18
#define F_ERR_NOT_ENOUGH_SPACE   19
#define F_ERR_NO_READING         20
#define F_ERR_NO_WRITING         21
#define F_ERR_DEL_OPENFILES      22
#define F_ERR_BAD_FILENAME       23
#define F_ERR_DUPLICATE_FILENAME 24

static char *FileError[] = {
	"OK",
	"Must format flash before use",
	"Must init KFS before use",
	"File system must be at least 4 segments",
	"spi_flash_erase error",
	"spi_flash_read error",
	"spi_flash_write error",
	"CRC error",
	"FAT memory allocation error",
	"SaveFAT: pointer is empty",
	"Too many open files (6 max)",
	"Invalid mode supplied for FileOpen",
	"File not found",
	"File is already open",
	"Cannot resize file: expandable file is open",
	"Cannot create file: expandable file is open",
	"Cannot create file: FAT limit",
	"File buffer memory allocation error",
	"Invalid t_File handle",
	"Not enough space",
	"Cannot read from write-only file",
	"Cannot write to read-only file",
	"Cannot delete: some files are open",
	"Invalid file name",
	"Duplicate file name"
};

// Flash file structure ----------------------------------

// FAT file record -- MUST BE 4-byte aligned in size
typedef struct {       // sizeof = 48 bytes
	char name[MAX_FILENAME_LEN]; // 32 bytes
	uint32 datetime;     // UNIX
	uint32 size;         // file size  : 30 bits max
	uint16 segment;      // segment number
	uint16 offset;       // offset (12 bits)
	uint32 head;         // write head : 30 bits max + 1 bit CYCLE flag (MSB)
} t_FileRec; // max 85 files available (assuming FAT to be 4096 bytes)

// FAT structure
typedef struct {    // max t_FAT header sizeof = (4096-4) - 85 /files/ * 48 /sizeof(t_FileRec)/  = 12 bytes
	// header
	uint8   version;     // 1 cyclic
	uint8   nfiles;      // 1 number of files
	uint16  crcd;        // 2 last CRC'd sector
	uint32  shift;       // 4 number of bytes to shift left on shrinking. If 0 then no shrinking is performed.
	uint16  segment;     // 2 destination segment for file being moved (soon to become normal file location)
	uint16  offset;      // 2 destination offset for file being moved (soon to become normal file location)
	// file records
	t_FileRec file[MAX_FILES]; //
} t_FAT;

//
typedef struct {       // sizeof = 187 bytes
	t_File  common;      // name, mime, date, size
	uint16  segment;     // segment number
	uint16  offset;      // offset
	uint32  head;        // write head : 30 bits max + 1 bit CYCLE flag (MSB)
		// runtime fields
	uint8   wrapped;     // {pos} wrapped attribute
	uint8   mode;        // (1..6) see KFS_Open
	uint8   fatindex;    // index for FAT record
	bool    expandable;  // file is expandable
	uint8   changes;     // 1 = data changed (data buffer needs to be saved), 2 = head changed (FAT needs to be saved)
} t_FileNode; // internal file structure


// ============================================================================
// INTERNAL VARS
//
static char errorMessage[128];         // error message buffer
static char fileExtension[48];

static uint16 kfs_start_seg = 0;       // start segment
static uint16 kfs_length_seg = 0;      // file system area length in segments, including SYS_SEGMENTS
static uint16 skip_segment[16];        // skip segments

static uint32 fileError = F_ERR_OK;    // last error code
static uint32 fileErrorExt = 0;

static char* fatSegment = NULL;         // 4K buffer which stores FAT while performing file operations (unaligned)
static char* rwSegment = NULL;          // 4K buffer which stores segment that has been read from flash (unaligned)
static char* rSegment = NULL;           // 4K buffer used while file deletion

static uint16 rwSegmentNo = 0;          // number of cached segment
static uint16 rSegmentNo = 0;           // number of cached segment
static bool  segmentBacked = false;    // cached segment backed up

static t_FileNode openFile[MAX_OPEN_FILES]; // list of open files
static uint8 nOpenFiles;                    // number of open files

static t_Dir dir;
static t_Stats stats;

static uint16 crcd = 1;                 // FAT local copy
static uint8 fat_nfiles;                // FAT local copy

static t_FileRec lastFile;              // FAT local copy: last file

// ============================================================================
// Get file extension (without dot)
//
char*   FLASH_CODE KFS_FileExtension ( char* fname )
{
  char* d = fileExtension;
  char* p = fname;

  while (*p) p++;
  while (p > fname && *p!='.') p--;
  if (*p=='.') p++;
  while (*p) *d++ = *p++;
  *d = 0;
  return fileExtension;  
}

// ============================================================================
// INTERNAL FUNCTION DECLARATIONS
//
static bool   RAM_CODE ReadFlash (char *buf, uint16 segment, uint16 offset, uint32 length);
static bool   RAM_CODE ReadSegment (uint16 segment, char *buf);
static bool   RAM_CODE WriteSegment (uint16 segment, char *buf);
static bool   RAM_CODE BackupSegment (void);
static bool   RAM_CODE SaveChanges (void);
//static char*  RAM_CODE ReadFlash (char *dst, char *src, uint32 len);
static t_FAT* FLASH_CODE OpenFAT ( void );
static bool   FLASH_CODE SaveFAT ( uint8 num );
static void   FLASH_CODE CloseFAT ( void );
static void   FLASH_CODE TryFree( void );
static uint8 FLASH_CODE SeekFAT ( char* fname, uint8 ifile, t_FileNode* frec, uint8* total );

// ============================================================================
// Read flash without CRC control
//
//  In: buf     : where to put data
//      segment : segment to read [0...]
//      segment : offset to read [0...]
//      length  : length of data
//
// Out: true / false
//
static bool RAM_CODE ReadFlash (char *buf, uint16 segment, uint16 offset, uint32 length)
{
	uint32 addr;

	while (length) {
		// acually read segment
		uint32 len = length;
		if (offset + len > DATA_SIZE) len = DATA_SIZE - offset;
		if (len==0) { segment++; offset=0; len = _MIN(length,DATA_SIZE); }
		//
		addr = ((uint32)kfs_start_seg + (uint32)segment) * SEGMENT_SIZE + offset;
		fileErrorExt = segment;
		fileError = F_ERR_FLASH_READ;
		if (spi_flash_read(addr, (uint32*)buf, len) != SPI_FLASH_RESULT_OK) { os_printf("  spi read error (%u:%u)\n", segment, offset); return false; }
	}

	return true;
}

// ============================================================================
// Read 4k *segment* with CRC control
//
//  In: segment : segment to read [0...]
//      buf     : where to put data
//
// Out: true / false
//

static int segmentMap[8][2];
static int nSegmentMap = 0;

static bool RAM_CODE ReadSegment (uint16 segment, char *buf)
{
	uint32 addr, crc_calc, crc_read;
	bool b;

	if (segment > 2) {
		if (buf==rwSegment && rwSegmentNo==segment) return true; // already in memory
		if (buf==rSegment && rSegmentNo==segment) return true; // already in memory
		if (buf==rwSegment && rwSegmentNo > 2 && segment != rwSegmentNo) // reading data segment, need to save
			if (!SaveChanges()) return false;
	}

	if (segment <= crcd) {
		// acually read segment
		addr = ((uint32)kfs_start_seg + (uint32)segment) * SEGMENT_SIZE;
		fileErrorExt = segment;
		fileError = F_ERR_FLASH_READ;
		if (spi_flash_read(addr, (uint32*)buf, SEGMENT_SIZE) != SPI_FLASH_RESULT_OK) { os_printf("  spi read error (addr = %08X, destination = %08X)\n", addr, buf); return false; }
		crc_calc = crc32c(buf, DATA_SIZE);
		crc_read = *(uint32*)(buf+SEGMENT_SIZE-CRC_SIZE);

		b = crc_calc == crc_read;
		fileError = b ? F_ERR_OK : F_ERR_CRC;
	} else {
		os_printf("ReadSegment(%d): using empty buffer\n", segment);
		// just use empty buffer
		b = true;
	}

	if (buf==rwSegment) { rwSegmentNo = segment; segmentBacked = false; }
	if (buf==rSegment)  rSegmentNo  = segment;
	
	return b;
}

// ============================================================================
// Write 4k segment with CRC control
//
// Out: true / false
//
static bool RAM_CODE WriteSegment (uint16 segment, char *buf)
{
	uint32 crc32, addr;
	uint16 seg = segment;
	char* msg[] = {"ERR","T/O","???"};
	uint32 i;

	fileErrorExt = segment;
os_printf("WriteSegment(%d)\n", segment);
	// calculate CRC for new segment
	crc32 = crc32c(buf, DATA_SIZE);
	*(uint32*)(buf+SEGMENT_SIZE-CRC_SIZE) = crc32;

	// erase sector
	segment += kfs_start_seg;
	SpiFlashOpResult r = spi_flash_erase_sector(segment);
	if (r!=SPI_FLASH_RESULT_OK) {
		if (r==SPI_FLASH_RESULT_ERR) i=0;
		else if (r==SPI_FLASH_RESULT_TIMEOUT) i=1;
		else i=2;
		os_printf("  err: flash erase (%u)(%s)\n", segment, msg[i]);
		fileError = F_ERR_FLASH_ERASE;
		return FALSE;
	}

	// write sector
	addr = segment * SEGMENT_SIZE;
	r = spi_flash_write(addr, (uint32*)buf, SEGMENT_SIZE);
	if (r!=SPI_FLASH_RESULT_OK) {
		if (r==SPI_FLASH_RESULT_ERR) i=0;
		else if (r==SPI_FLASH_RESULT_TIMEOUT) i=1;
		else i=2;
		os_printf("  err: flash write (%s)\n", msg[i]);
		fileError = F_ERR_FLASH_WRITE;
		return FALSE;
	}
	r = spi_flash_read(addr, (uint32*)buf, SEGMENT_SIZE);
	if (r!=SPI_FLASH_RESULT_OK) {
		os_printf("backread failed\n");
	}
	// update last CRC'd segment if needed
	if (crcd < seg) crcd = seg;

	fileError = r==SPI_FLASH_RESULT_OK ? F_ERR_OK : F_ERR_FLASH_WRITE;
	return r==SPI_FLASH_RESULT_OK;
}

// ============================================================================
// Back up 4k segment before writing operations
//
// Out: true / false
//
static bool RAM_CODE BackupSegment (void)
{
	if (segmentBacked) return true;
	*(uint16*)(rwSegment+DATA_SIZE) = rwSegmentNo;
//	os_printf("backing up current segment\n");
	if (!WriteSegment(TEMP_SEGMENT, rwSegment)) return false;
	segmentBacked = true;
	return true;
}

// ============================================================================
// Save changes in data / FAT
//
// Out: true / false
//
static bool RAM_CODE SaveChanges (void)
{
	bool save_fat = 0, save_data = 0;
	uint16 i;
	t_FileNode* ff;

	// look for changes
	for (i=0; i<MAX_OPEN_FILES; i++) {
		ff = &openFile[i];
		if (ff->common.name[0] && ff->changes) {
			if (ff->changes & 1) { save_data++; }
			if (ff->changes & 2 && !ff->expandable) { save_fat++; }
		}
	}

	if (save_data) {
		os_printf("SaveChanges() : save data\n");
		if (!WriteSegment(rwSegmentNo, rwSegment)) return false;
	}
	
	if (save_fat) {
		os_printf("SaveChanges() : save fat\n");
		t_FAT* fat = OpenFAT();
		if (!fat) return false;
		for (i=0; i<MAX_OPEN_FILES; i++) {
			ff = &openFile[i];
			os_printf("  open file \"%s\"\n", ff->common.name);
			if (ff->common.name[0] && (ff->changes & 2)) {
				os_printf("    saving file \"%s\", fat index = %d, head = %d, size = %d\n", ff->common.name, ff->fatindex, ff->head, ff->common.size);
				os_strncpy(fat->file[ff->fatindex].name, ff->common.name, MAX_FILENAME_LEN);
				fat->file[ff->fatindex].name[MAX_FILENAME_LEN] = 0;
				fat->file[ff->fatindex].segment = ff->segment;
				fat->file[ff->fatindex].offset = ff->offset;
				fat->file[ff->fatindex].head = ff->head;
				fat->file[ff->fatindex].size = ff->common.size;
			}
		}
		if (!SaveFAT(0)) return false; 
		if (!SaveFAT(1)) return false;
		CloseFAT();
	}

	for (i=0; i<MAX_OPEN_FILES; i++) {
		if (save_data) openFile[i].changes &= 0xFD;
		if (save_fat)  openFile[i].changes &= 0xFE;
	}

	return true;
}

// ============================================================================
// Allocate segment buffer
//
bool FLASH_CODE SegmentBufferAllocate (void)
{
	if (!rwSegment) {
		rwSegment = k_malloc(SEGMENT_SIZE+4, "segmentMem", "SegmentBufferAllocate");
		if (!rwSegment) {
			fileError = F_ERR_FILE_ALLOC;
			return false;
		}
		rwSegmentNo = 0;
	}
	return true;
}

// ============================================================================
// Free segment buffer
//
void FLASH_CODE SegmentBufferFree (void)
{
	if (rwSegment) k_free(rwSegment);
	rwSegment = NULL;
}

// ============================================================================
// Read flash data using 4-byte aligned reads
//
// Out: true / false
//
/*
static char* RAM_CODE ReadFlash (char *dst, char *src, uint32 len)
{
	uint32 b,dw;
	char *s = src - ((uint32)src&3);
	dw = *(uint32*)s;
	while (len--) {
		b = src - s; // 0..3
		*dst++ = dw >> (3 - (src-s))*8;
		if (b==3) dw = *(s+=4);
		src++;
	}
}
*/

// ============================================================================
// Read most recent FS error code
//
uint8 FLASH_CODE KFS_Error (void)
{
	return fileError;
}

// ============================================================================
// Get most recent FS error message
//
char* FLASH_CODE KFS_ErrorMessage (void)
{
	if (fileError == F_ERR_FLASH_ERASE ||
			fileError == F_ERR_FLASH_READ ||
			fileError == F_ERR_FLASH_WRITE ||
			fileError == F_ERR_CRC
	) {
		os_sprintf(errorMessage, "%s at segment %u", FileError[fileError], fileErrorExt);
		return errorMessage;
	} else return FileError[fileError];
}

// ============================================================================
// KFS File system format:
//
//   Segment  Description
//   0        FAT #1
//   1        FAT #2 which normally is a copy or FAT#1
//   2        temporary backup segment for write operations
//   3+       data segments
//
//   Segment format:
//   4096-8 bytes  : data
//        4 bytes  : next position (used on delete)
//        4 bytes  : CRC32
//
// FAT format:
//
//   header              (see t_FAT)
//   file records []     (see t_FileRec)
//
//   Note: file record [0] is reserved for write/relocate operations
//
// File data format:
//
//   0..63         : mime type (asciiz)
//   64+           : binary data
//
// ============================================================================


// ============================================================================
// Format file system
//
// Write empty FATs
//
// Out: true / false
//
bool FLASH_CODE KFS_Format ( uint32 start_seg, uint32 length_seg)
{
	kfs_start_seg = start_seg;
	kfs_length_seg = length_seg;
	uint32 i;

	os_printf("KFS_Format(0x%X,0x%X)\n", start_seg, length_seg);

	if (length_seg <= SYS_SEGMENTS) { fileError = F_ERR_FS_LENGTH; return false; }

	t_FAT *fat = OpenFAT();
	if (!fat) return false;

	fat_nfiles     = 0; // local copy
	crcd           = 1; // local copy
	fat->version   = 0;
	fat->shift     = 0;
	fat->segment   = 0;
	fat->offset    = 0;
	if (!SaveFAT(0)) return false;
	if (!SaveFAT(1)) return false;
	CloseFAT();

	return true;
}

// ============================================================================
// Check FS integrity and perform relevant actions as needed
//
bool FLASH_CODE KFS_Init ( uint32 start_seg, uint32 length_seg )
{
	os_printf("KFS_Init( starting at segment %02X, length = %u bytes )\n", start_seg, length_seg * SEGMENT_SIZE);

	if (length_seg <= SYS_SEGMENTS) { fileError = F_ERR_FS_LENGTH; return false; }

	kfs_start_seg = start_seg;
	kfs_length_seg = length_seg;

	uint8 i;
	for (i=0; i<MAX_OPEN_FILES; i++) openFile[i].common.name[0] = 0;
	nOpenFiles = 0;

	if (!OpenFAT()) return false;
	if (!SegmentBufferAllocate()) return false;

	if (!ReadSegment(0, fatSegment)) return false;
	if (!ReadSegment(1, rwSegment)) return false;

	t_FAT* fat1 = (t_FAT*)fatSegment;
	t_FAT* fat2 = (t_FAT*)rwSegment;

	// check for FS
	bool correct_fs = false;
	uint16 v1 = fat1->version, v2 = fat2->version, n1 = fat1->nfiles, n2 = fat2->nfiles;
os_printf("v1 = %d, v2 = %d, n1 = %d, n2 = %d\n", v1, v2, n1, n2);

	if (_ABS(v1-v2) <= 1 && n1==n2) {
		// TODO: read FAT, check for interrupted write operations, fix if needed
		correct_fs = true;
		crcd = fat1->crcd;
		fat_nfiles = fat1->nfiles;
	}

	SegmentBufferFree();
	CloseFAT();  

	t_FileNode finfo;
	SeekFAT("", 0, &finfo, &fat_nfiles);

	fileError = (correct_fs ? F_ERR_OK : F_ERR_FORMAT);
	return correct_fs;
}

// ============================================================================
// Open FAT
//
static t_FAT* FLASH_CODE OpenFAT ( void )
{
	// allocate buffer
	if (!fatSegment) {
		fatSegment = k_malloc(SEGMENT_SIZE + 4, "fatSegment", "OpenFAT");
		if (!fatSegment) { fileError = F_ERR_FAT_ALLOC; return NULL; }
		// read FAT #1
		if (!ReadSegment(0, fatSegment)) return NULL;
	}
	fileError = F_ERR_OK;
	return (t_FAT*)fatSegment;
}

// ============================================================================
// Save FAT
//
// In:   num : table number (0..1)
//
static bool FLASH_CODE SaveFAT ( uint8 num )
{
	if (num>1) return false;
	if (!fatSegment) { fileError = F_ERR_FAT_EMPTY; return false; }
	os_printf("writing crcd=%d, nfiles=%d\n", crcd, fat_nfiles);
	((t_FAT*)fatSegment)->crcd = crcd;
	((t_FAT*)fatSegment)->nfiles = fat_nfiles;
	if (!WriteSegment(num, fatSegment)) return false;
	fileError = F_ERR_OK;
	return true;
}

// ============================================================================
// Deallocate FAT buffer
//
static void FLASH_CODE CloseFAT ( void )
{
	if (fatSegment) k_free(fatSegment);
	fatSegment = NULL;
}

// ============================================================================
// Seek FAT for a specified {fname}, return file record, total number of files and "last file" flag
//
// In:   fname  : file to find
//                  empty string "" to get last file in FAT
//                  NULL to read specific file by index
//       ifile  : index of file record to read
//       frec   : (OUT) where to put file record
//       total  : (OUT) number of files
//
// Out:  found flag 
//       0 = not found
//       1 = found
//       2 = found, last file (expandable)
//
static uint8 FLASH_CODE SeekFAT ( char* fname, uint8 ifile, t_FileNode* finfo, uint8* total )
{
	char fatrec[sizeof(t_FileRec) * 5];
	uint8 nfiles, i, index;
	uint32 addr, clen, rlen;
	uint8 found = 0;
	t_FileRec* frec = NULL;

	fileErrorExt = 0;

os_printf("SeekFAT(");

	if (!fname) {
		os_printf("index=%d)\n",ifile);
		// just read file record by index
		addr = ((uint32)kfs_start_seg + (uint32)0) * SEGMENT_SIZE + OFFSETOF(t_FAT,file) + sizeof(t_FileRec)*ifile;
		fileError = F_ERR_FLASH_READ;
		if (spi_flash_read(addr, (uint32*)fatrec, sizeof(t_FileRec)) != SPI_FLASH_RESULT_OK) return found;
		frec = (t_FileRec*)fatrec;
		found = 1;
	} else {
		os_printf("file=\"%s\")\n",fname);
		// read nfiles
		nfiles = fat_nfiles;
		*total = nfiles;
		addr = ((uint32)kfs_start_seg + (uint32)0) * SEGMENT_SIZE + OFFSETOF(t_FAT,file);

		// read file records
		uint8 rd = 0;
		while (rd < nfiles && !frec) {
			clen = _MIN(nfiles-rd, 5);
			fileError = F_ERR_FLASH_READ;
			rlen = clen*sizeof(t_FileRec);
			if (spi_flash_read(addr, (uint32*)fatrec, rlen) != SPI_FLASH_RESULT_OK) return found;

			t_FileRec* r = (t_FileRec*)fatrec;
			for (i=0; i<clen; i++) {
				if (*fname==0) {
					// find by index
					if (rd+i == nfiles-1) {
						frec = (t_FileRec*)(&r[i]);
						index = rd+i;
						found = 2; 
						break; 
					}
				} else {
					// find by name
					if (os_strncmp(fname,r[i].name,MAX_FILENAME_LEN)==0) { 
						frec = (t_FileRec*)(&r[i]);
						index = rd+i;
						found = rd+i == nfiles-1 ? 2 : 1; 
						break; 
					}
				}
			}
			rd += clen;
			addr += rlen;
		}// while
	}

	if (frec) {
		os_printf("  found = %d\n", found);
		os_strncpy(finfo->common.name, frec->name, sizeof(finfo->common.name));
		finfo->common.datetime = frec->datetime;
		finfo->common.size     = frec->size;
		finfo->segment         = frec->segment;
		finfo->offset          = frec->offset;
		finfo->head            = frec->head;
		finfo->fatindex        = index;
	}

	fileError = F_ERR_OK;
	return found;
}

// ============================================================================
// Return PHYSICAL file position address based on {segment}+{offset} and {pos}.
//
// In:  segment : origin segment
//      offset  : origin offset
//      pos     : logical position inside the file, (i.e. from actual data start) (min=0, max=filesize-1)
//      mode    : 0 = EXT_FILE = normal, 1 = EXT_NEXT = new file placement
//
static uint32 FLASH_CODE ExtAddr (uint32 segment, uint32 offset, uint32 pos, uint8 mode)
{
	uint32 start, startoff, addr;
//os_printf("ExtAddr(%u, %u, %u, %s)", segment, offset, pos, mode==EXT_FILE?"EXT_FILE":"EXT_NEXT");
	start = segment * SEGMENT_SIZE + offset + MIME_SIZE;
	startoff = start % SEGMENT_SIZE;
	if (startoff + pos >= DATA_SIZE) {
		pos -= DATA_SIZE - startoff;
		addr = start + (SEGMENT_SIZE - startoff) + pos + (pos / DATA_SIZE) * CTRL_SIZE;
	} else {
		addr = start + pos;
	}

	if (mode==EXT_NEXT)                                               // looking for the beginning of NEXT file
		if (addr % SEGMENT_SIZE > DATA_SIZE - MIME_SIZE)  // if there is no place for MIME then skip to the next segment
			addr += SEGMENT_SIZE - addr % SEGMENT_SIZE;
//os_printf(" = (%u : %u)\n", addr/SEGMENT_SIZE, addr%SEGMENT_SIZE);
	return addr;
}

// ============================================================================
// Get file system statistics
//
t_Stats* FLASH_CODE KFS_Stats ( void )
{
	t_FAT* fat = OpenFAT();
	if (!fat) return NULL;

	uint32 i, sz = 0;
	for (i=0; i<fat->nfiles; i++) sz += fat->file[i].size;

	i = 0;
	if (fat->nfiles) {
		i = ExtAddr( fat->file[fat->nfiles-1].segment, fat->file[fat->nfiles-1].offset, fat->file[fat->nfiles-1].size, EXT_NEXT );
		i -= SYS_SEGMENTS * SEGMENT_SIZE;
	}

	stats.start_segment = kfs_start_seg;
	stats.length_segments = kfs_length_seg;
	stats.total  = (kfs_length_seg - SYS_SEGMENTS) * DATA_SIZE;
	stats.used   = (i/SEGMENT_SIZE) * DATA_SIZE + (i % SEGMENT_SIZE);
	stats.data   = sz;
	stats.nfiles = fat->nfiles;

	return &stats;
}

// ============================================================================
// Next file in directory
//
// Out: false = no more files to list
//
static bool FLASH_CODE KFS_DirNext ( void )
{
	t_FileNode finfo;
	if (dir.pos==dir.len) return false;

	SeekFAT(NULL, dir.pos, &finfo,  NULL); // get total number of files

	os_strcpy(dir.file.name, finfo.common.name);
	dir.file.mime[0] = 0;
	dir.file.datetime = finfo.common.datetime;
	dir.file.size = finfo.common.size;
	dir.pos++;
	return true;
}

// ============================================================================
// Open directory
//
// Out: dir struct
//
t_Dir* FLASH_CODE KFS_OpenDir ( char* name )
{
	os_strcpy( dir.name, name ); // TODO: use it
	os_printf("OpenDir: nfiles=%d\n", fat_nfiles);
	dir.pos = 0;
	dir.len = fat_nfiles;
	dir.Next = KFS_DirNext;
	return &dir;
}

// ============================================================================
// Close directory
//
void FLASH_CODE KFS_CloseDir (void)
{
	TryFree();
}

// ============================================================================
// Cut filename to MAX_FILENAME_LEN chars (with UTF-8 support) keeping file extension intact
// 
// In:  fname    : filename to cut
//      cutfname : target buffer
//
void FLASH_CODE FileNameCut (char* fname, char* cutfname)
{
	char ch, *eos, *p, *ext = 0;
	uint8 b, bytes, fnlen, extlen;
	// find ext
	eos = fname; while (*eos) eos++;
	fnlen = eos - fname;
	if (fnlen > MAX_FILENAME_LEN) {
		p = eos;
		while (p > fname) if (*p=='.') { ext = p; break; } else p--;
		extlen = ext ? eos - p : 0;
		// count utf-8 chars
		bytes = 0; p = fname;
		while (bytes < MAX_FILENAME_LEN - extlen) {
			ch = *p;
			b = 0;
			if ((ch & 0x80) == 0) b = 1;
			else if ((ch & 0xC0) == 0xC0) b = 2;
			else if ((ch & 0xE0) == 0xE0) b = 3;
			else if ((ch & 0xF0) == 0xF0) b = 4;
			bytes += b;
			p += b;
		}
		if (bytes > MAX_FILENAME_LEN - extlen) bytes -= b;
		os_strncpy(cutfname, fname, bytes);
		os_strcpy(cutfname+bytes, ext);
		//
	} else {
		os_strcpy(cutfname, fname);
	}
}

// ============================================================================
// Check if file exists
//
// Out: true/false
//
bool FLASH_CODE KFS_FileExists ( char* name )
{
	os_printf("KFS_FileExists()\n");
	t_FileNode finfo;
	uint8 nfiles;
	char cutname[MAX_FILENAME_LEN+1];
	if (*name=='/') name++;
	FileNameCut(name, cutname);
	return SeekFAT(cutname, 0, &finfo, &nfiles) > 0;
}

// ============================================================================
// Create file in KFS
//
// In:  name : file name
//      mode : r, w, a, r+, w+, a+
//      mime : mime type (for w, a, w+, a+)
//      size : file size (for w, a, w+) :: if set to 0 then file is expandable until closed.
// Modes:
//      r   (1) : Reading. File must exist. Seek to 0.
//      w   (2) : Writing. File is created if not exists, overwritten if exists. Seek to 0.
//      a   (3) : Append.  File is created if not exists. Seek to EOF. <- head works here!
//      r+  (4) : Update both reading & writing. File must exist. Seek to 0.
//      w+  (5) : Create for reading & writing. Created if not exists, overwritten if exists. Seek to 0.
//      a+  (6) : Open for reading & appending. File must exist. Seek to EOF. <- head works here!
//
// Out: t_File* / NULL
//
static void FLASH_CODE TryFree()
{
	if (nOpenFiles==0) {
		SegmentBufferFree();
		CloseFAT();
	}
}
t_File* FLASH_CODE KFS_Open (char* name, char* mode, char* mime, uint32 size)
{
os_printf("KFS_Open(\"%s\", \"%s\")\n", name, mode);
	uint32 i;
	uint8 ff, nfiles, ofile, exp = 0;
	// check if file system is inited
	if (kfs_start_seg == 0) { fileError = F_ERR_INIT; return NULL; }
	// check file name
	if (!name || name[0]==0 || name[0]==' ') { fileError = F_ERR_BAD_FILENAME; return false; }
	// check modes
	uint8 rwa = 0;
	fileError = F_ERR_BAD_OPEN_MODE;
	if (!mode) return NULL;
	if (*mode==0) return NULL;
	if (*mode=='r') rwa = 1;
	if (*mode=='w') rwa = 2;
	if (*mode=='a') rwa = 3;
	if (!rwa) return NULL;
	if (*(mode+1)=='+') rwa += 3;

	char cutname[MAX_FILENAME_LEN+1];
	FileNameCut(name, cutname);

	// check if too many files were open
	if (nOpenFiles == MAX_OPEN_FILES) { os_printf("  err: too many open files\n"); fileError = F_ERR_TOO_MANY_OPEN; return NULL; }
	ofile = 0;
	i = 0; // reserved
	char* fname;
	for (ff=0; ff<MAX_OPEN_FILES; ff++) {
		fname = openFile[ff].common.name;
		if (*fname && openFile[ff].expandable) exp++; // already have open expandable file
		if (os_strncmp(fname, cutname, MAX_FILENAME_LEN)==0) { os_printf("  err: already open\n"); fileError = F_ERR_ALREADY_OPEN; return NULL; }
		if (*fname==0 && !i) {
			ofile = ff;
			i = 1;
		}
	}
	t_FileNode* fn = &openFile[ofile]; // get open file slot

	t_FileNode finfo;
	uint8 found = SeekFAT(cutname, 0, &finfo, &nfiles);

	// file must exist? (r, r+, a+)
	if (!found && (rwa==1 || rwa==4 || rwa==6)) { TryFree(); fileError = F_ERR_FILE_NOT_FOUND; return NULL; }

	uint8 create = 0;
	// file not found? check for file number limit
	if (!found) {
		if (nfiles == MAX_FILES) { TryFree(); fileError = F_ERR_FAT_IS_FULL; return NULL; }
		create = 1;
	}
	// file is found and we are about to overwrite it (w, w+) AND new size is different
//	os_printf("found=%d, rwa=%d, fat->file[ff].size = %d, size = %d, fat->file[ff].head = %d\n", found, rwa, fat->file[ff].size, size, fat->file[ff].head);
	if (found && (rwa==2 || rwa==5)) {
		if (finfo.common.size == size) {
			finfo.head = 0;      // reset head
		} else {
			// last file on flash? do not create
			if (found==2) {
				// check if new file size exceeds disk space limit
				if ( ExtAddr( finfo.segment, finfo.offset, size, EXT_FILE ) / SEGMENT_SIZE >= kfs_length_seg ) { TryFree(); fileError = F_ERR_NOT_ENOUGH_SPACE;  return NULL; }
				finfo.common.size = size;   // set new size
				finfo.head = 0;      // reset head
			} else {
				// size mismatch: delete old
				if (exp) { fileError = F_ERR_EXP_NO_RESIZE; return NULL; }
				if (!KFS_Delete(cutname)) return NULL; // fat_nfiles is decreased here
				nfiles--;
				create = 1;
			}
		}
	}

	uint32 segm, offs;

	fn->changes = 0;
	fn->expandable = (uint8)((found==0 || found==2) && size == 0); // last file and required size = 0; in this case previous (or newly set) file size does not matter
	if (create && exp) { fileError = F_ERR_EXP_NO_CREATE; return NULL; }

	if (create) {
		// seek to the end of the last file...
		t_FileNode flast;
		uint8 last = SeekFAT("", 0, &flast, &nfiles);
		os_printf("last = %d\n", last);
		uint32 bound = ( last==0 ? SYS_SEGMENTS*SEGMENT_SIZE : ExtAddr( flast.segment, flast.offset, flast.common.size, EXT_NEXT ) ); // physical record start
		os_printf("bound = %X\n", bound);
		uint32 bound2 = ExtAddr( bound / SEGMENT_SIZE, bound % SEGMENT_SIZE, size, EXT_FILE ); // physical record end
		os_printf("bound2 = %X\n", bound2);
		// flash full?
		if (bound2 / SEGMENT_SIZE >= kfs_length_seg) { TryFree(); fileError = F_ERR_NOT_ENOUGH_SPACE;  return NULL; }
		// ...and make it the start of a new file
		fn->segment = bound / SEGMENT_SIZE;
		fn->offset = bound % SEGMENT_SIZE;
//    fn->bound = ExtAddr( frec.segment, frec.offset, size, EXT_FILE );
		// overwrite: (re)create file data
		os_printf("copying \"%s\" to fn\n", cutname);
		os_strncpy(fn->common.name, cutname, sizeof(fn->common.name)); fn->common.name[MAX_FILENAME_LEN] = 0;
		os_strncpy(fn->common.mime, mime, sizeof(fn->common.mime)); fn->common.mime[MAX_MIMETYPE_LEN] = 0;
		fn->common.datetime = GetPosixTime();
		fn->common.size     = size;
		fn->head            = 0;
		fat_nfiles++;

		// TODO: save data to FAT

		/*
		// copy file attributes from FileNode to FAT record
		os_strncpy(fat->file[ff].name, fn->common.name, sizeof(fat->file[ff].name)); // 32 bytes
		fat->file[ff].datetime = fn->common.datetime;
		fat->file[ff].size     = fn->common.size;
		fat->file[ff].segment  = fn->segment;
		fat->file[ff].offset   = fn->offset;
		fat->file[ff].head     = fn->head;
		fat->nfiles++;
		*/
	} else {
		// existing file: mark bound
//    fn->bound = ExtAddr( fn->segment, fn->offset, fn->common.size, EXT_FILE );
		// read (file must exist): copy FAT data into file node
		os_strncpy( fn->common.name, finfo.common.name, MAX_FILENAME_LEN ); fn->common.name[MAX_FILENAME_LEN] = 0;
		fn->common.datetime = finfo.common.datetime;
		fn->common.size     = finfo.common.size;
		fn->head            = finfo.head;
		fn->segment         = finfo.segment;
		fn->offset          = finfo.offset;
	}

	fn->fatindex = found ? finfo.fatindex : nfiles;
	fn->common.pos = rwa==3 || rwa==6 ? fn->head & 0x7FFFFFFF : 0; // logical : (a,a+) => EOF, (other) => 0
	fn->wrapped = (rwa==3 || rwa==6) && fn->head & 0x80000000 ? 1 : 0; // means {pos} already wrapped on read/write
	fn->mode = rwa;

	if (!SegmentBufferAllocate()) return NULL;

	if (create) {
		// write mime type:
		// TODO: backup segment!!!!!
		char mime[MIME_SIZE];
		if (os_strncmp(fn->common.mime, "application/", 12)==0) {
			mime[0] = '%';
			os_strncpy(mime+1, fn->common.mime+12, sizeof(mime)-1);
		} else os_strncpy(mime, fn->common.mime, sizeof(mime));
		if (!ReadSegment(fn->segment, rwSegment)) return 0;   // read segment
		os_strncpy(rwSegment+fn->offset, mime, sizeof(mime));
	} else {
		// read mime type:
		char _mime[MIME_SIZE+32];
		if (!ReadSegment(fn->segment, rwSegment)) return 0;   // read segment
		if (*(rwSegment+fn->offset)=='%') {
			os_strcpy(fn->common.mime, "application/");
			i = os_strncpy(fn->common.mime+12, rwSegment+fn->offset+1, MIME_SIZE-12);
			*(fn->common.mime+12+i) = 0;
		} else {
			i = os_strncpy(fn->common.mime, rwSegment+fn->offset, MIME_SIZE);
			*(fn->common.mime+i) = 0;
		}
	}

	nOpenFiles++;

//os_printf("File \"%s\" is open, mode = %d, size = %d, head = %d, pos = %d, wrapped = %d\n", fn->common.name, rwa, fn->common.size, fn->head, fn->common.pos, fn->wrapped);

	return (t_File*)fn;
}

// ============================================================================
// Close file
//
// Out: t_File* / NULL
//
bool FLASH_CODE KFS_Close (t_File *f)
{
	uint8 i;
os_printf("KFS_Close()\n");
	for (i=0; i<MAX_OPEN_FILES; i++) {
		if (&openFile[i] == (t_FileNode*)f) break;
	}
	if (i==MAX_OPEN_FILES) { fileError = F_ERR_BAD_HANDLE; return false; }

	t_FileNode* fn = (t_FileNode*)f;

	//os_printf("closing file \"%s\", mode = %d, changes = %s %s, expandable = %s\n", f->name, fn->mode, fn->changes & 1 ? "DATA" :"", fn->changes & 2 ? "FAT" :"", fn->expandable ? "YES" : "NO");
	fn->expandable = 0; // to save FAT
	if (fn->mode != 1) SaveChanges();

	fn->common.name[0] = 0;
	nOpenFiles--;
	TryFree();

	return true;
}

// ============================================================================
// Rename file
//
// Out: true / false = success / error
//
bool FLASH_CODE KFS_Rename (char* oldname, char* newname)
{
	uint16 ff;
	char cutname_old[MAX_FILENAME_LEN+1], cutname_new[MAX_FILENAME_LEN+1];

	if (!oldname || oldname[0]==0) { fileError = F_ERR_FILE_NOT_FOUND; return false; }
	if (!newname || newname[0]==0) { fileError = F_ERR_BAD_FILENAME; return false; }

	FileNameCut( oldname, cutname_old );
	FileNameCut( newname, cutname_new );

	if (os_strcmp(cutname_old, cutname_new)==0) return true; // same name

	t_FAT* fat = OpenFAT();
	if (!fat) return false;

	// find file in FAT by name
	for (ff=0; ff<fat->nfiles; ff++) {
		if (os_strncmp(cutname_new, fat->file[ff].name, MAX_FILENAME_LEN)==0) { fileError = F_ERR_DUPLICATE_FILENAME; return false; }
	}
	for (ff=0; ff<fat->nfiles; ff++) {
		if (os_strncmp(cutname_old, fat->file[ff].name, MAX_FILENAME_LEN)==0) break;
	}

	if (ff == fat->nfiles) { fileError = F_ERR_FILE_NOT_FOUND; return false; }

	os_strncpy(fat->file[ff].name, cutname_new, sizeof(fat->file[ff].name));
	
	SaveFAT(0);
	SaveFAT(1);

	CloseFAT();

	return true;
}

// ============================================================================
// Seek file
//
//   Move read/write position within a file
//
//  In: f      : file handle
//      offset : number of bytes to offset from {origin}
//      origin : position used as a reference for the {offset}
//
// Out: new file position
//
uint32 FLASH_CODE KFS_Seek (t_File *f, sint32 offset, e_SeekOrigin origin)
{
	uint8 i;
	for (i=0; i<MAX_OPEN_FILES; i++) {
		if (&openFile[i] == (t_FileNode*)f) break;
	}
	if (i==MAX_OPEN_FILES) { fileError = F_ERR_BAD_HANDLE; return 0; }

	t_FileNode* fn = (t_FileNode*)f;

	//char* txt[] = {"SEEK_SET","SEEK_CUR","SEEK_HEAD","SEEK_END"};
	//os_printf("KFS_Seek(\"%s\", %d, %s), f->size=%d\n", f->name, offset, txt[origin], fn->common.size);

	if (origin!=KFS_SEEK_CUR && offset<0) offset = 0;

	if (offset > (sint32)fn->common.size) offset = fn->common.size;

	uint32 sz = fn->common.size ? fn->common.size : fn->head;

	switch (origin) {
		case KFS_SEEK_SET:
			fn->common.pos = _MIN( offset, sz );
		break;
		case KFS_SEEK_CUR:
			if (offset < 0) fn->common.pos = _MAX( 0, (sint32)fn->common.pos + offset );
			else fn->common.pos = _MIN( fn->common.pos + offset, sz );
		break;
		case KFS_SEEK_END:
			if (fn->head & 0x80000000) fn->common.pos = _MAX( fn->common.size - offset, 0 );
			else fn->common.pos = _MIN( fn->head - offset, 0 );
		break;
	}

	return fn->common.pos;
}

// ============================================================================
// Read/write file
//
//   Read file from current position {fn->common.pos} into {buf} and move position {len} bytes ahead.
//   Write file starting from current position {fn->common.pos} from {buf} and move position {len} bytes ahead.
//
//  In: f    : file handle
//      buf  : read buffer
//      len  : number of bytes to read/write
//      rw   : 0 = read, 1 = write
//
// Out:  0+  : number of bytes actually read/written
//
static sint32 FLASH_CODE KFS_RW (t_File *f, char* buf, uint32 len, uint8 rw)
{
	uint8 i;
	char* rwtxt[] = {"READ","WRITE"};

	for (i=0; i<MAX_OPEN_FILES; i++)
		if (&openFile[i] == (t_FileNode*)f) break;

	if (i==MAX_OPEN_FILES) { fileError = F_ERR_BAD_HANDLE; return 0; }

	t_FileNode* fn = (t_FileNode*)f;

	// read/write mode checks
	if (rw==0 && (fn->mode==2 || fn->mode==3)) { fileError = F_ERR_NO_READING; return 0; }  //
	if (rw==1 && fn->mode==1) { fileError = F_ERR_NO_WRITING; return 0; }

	// cache values
	uint32 head_wrapped = fn->head & 0x80000000;      // head wrapped
	uint32 head = fn->head & 0x7FFFFFFF;              // physical head position
	uint32 pos  = fn->common.pos;                     // logical position inside file (0..MAX(head,fsize))
	uint32 size = fn->common.size;                    // can be 0 or less then {head}
//os_printf("KFS_RW(\"%s\", %d, %s)\n", f->name, len, rwtxt[rw]);
//os_printf("  size = %d\n", size);
//os_printf("  pos = %d\n", pos);
	uint32 elen = head_wrapped ? size : head;         // effective file length
	if (rw==0 && pos==elen)  return 0;                // already at EOF
	if (rw==0 && len > elen-pos) len = elen-pos;      // do not read beyond EOF
	if (size==0 && !fn->expandable) return 0;         // zero-length file

	// {pos} base: {head} or 0 depending on {head} MSB
	uint32 segm, offs, addr, clen, bytes = 0;         //
	uint32 phys_pos = head_wrapped ? (head + pos) % size : pos; // physical offset for pos (from phys BOF)
//os_printf("rw = %u, head_wrapped = %u, head = %u, pos = %u, size = %u, elen = %u, bufsize = %u, phys_pos = %d\n", rw, head_wrapped, head, pos, size, elen, len, phys_pos);

	while (len > 0) {

		if (rw==0 && phys_pos==size) { phys_pos = 0; fn->wrapped = 1; }; // read: wrap at EOF

		uint32 addr = ExtAddr(fn->segment, fn->offset, phys_pos, EXT_FILE); // physical position
		uint32 segm = addr/SEGMENT_SIZE;
		uint32 offs = addr%SEGMENT_SIZE;
//    os_printf("segm = %02X, kfs_len_seg = %02X\n", segm, kfs_length_seg);
		if (segm >= kfs_length_seg) { os_printf("Segment fault (%u)\n", segm); fileError = F_ERR_NOT_ENOUGH_SPACE; return 0; }

		if (!ReadSegment(segm, rwSegment)) return 0;   // read segment

		// do not backup if expandable and segment does not contain previous files
		bool backup = fn->expandable && (segm > fn->segment || (segm == fn->segment && fn->offset==0)) ? false : (rw==2);
		if (backup) if (!BackupSegment()) return 0;

		// cut to segment boundary
		clen = _MIN( DATA_SIZE - offs, len );
		// cut to head
		if (rw==0 && phys_pos < head) clen = _MIN( head - phys_pos, clen );
		// cut to EOF (both r & w)
		if (!fn->expandable && phys_pos + clen > size) {
			clen = size - phys_pos;
		}

		if (clen) {
			if (rw==0) {
				os_memcpy( buf, rwSegment + offs, clen );       // read
			} else {
				os_memcpy( rwSegment + offs, buf, clen );       // write
				fn->changes |= 1; // data changed
			}
			buf += clen;
			len -= clen;
			phys_pos += clen;
			bytes += clen;
//			os_printf("phys_pos = %u, len = %u, size = %d, head = %d\n", phys_pos, len, size, head);
		}

		if (rw==0 && phys_pos >= head) { len = 0; }; // read: wrap at EOF
		if (rw==1 && phys_pos > head) {
			fn->changes |= 2; // FAT changed
			if (fn->expandable) {
				head = size = phys_pos;  // move head and size
			} else { // not expandable : size >> 0
				head = phys_pos % size;  // move head
				if (phys_pos == size) {  // just wrote to the end of file
					head_wrapped = fn->wrapped = 1;  // wrap head
					phys_pos = 0; // rewind back to physical BOF (right where the head now is!)
				}
			}
		}

		// adjust FAT data
		fn->head = head | (head_wrapped ? 0x80000000 : 0);
		fn->common.size = size;
		if (head_wrapped) {
			if (phys_pos > head) fn->common.pos = phys_pos - head;
			else if (fn->wrapped) fn->common.pos = size - head + phys_pos;
		} else {
			fn->common.pos = phys_pos;
		}

	}// while len > 0
	fileError = F_ERR_OK;
//os_printf("fn->common.pos = %u\nfn->head = %u", fn->common.pos, fn->head);
	return bytes;
}

// ============================================================================
// Delete file and shrink FS if needed
//
// In:  name : file name (with full path)
//
// Out: true = ok, false = error (see KFS_Error for more info)
//
char* FLASH_CODE GetSourceBuffer (uint16 segment)
{
	if (segment==rwSegmentNo) {
    os_printf("GetSourceBuffer: reading from rwSegment (i.e. %X)\n", rwSegmentNo);
		return rwSegment;
	}

	if (!rSegment) { rSegment = k_malloc(SEGMENT_SIZE, "rSegment", "GetSourceBuffer"); }
  os_printf("GetSourceBuffer: reading segment %X into rSegment\n", segment);
	if (!ReadSegment(segment, rSegment)) return NULL;

	return rSegment;
}
void DeleteSourceBuffer (void)
{
	if (rSegment) {
		k_free(rSegment);
		rSegment = NULL;
	}
}
bool FLASH_CODE KFS_Delete (char* name)
{
	// TODO: cannot delete file if some other files are open! (or reopen them transparently?)

	if (nOpenFiles > 0) {
		uint8 ff;
		for (ff=0; ff<MAX_OPEN_FILES; ff++) {
			if (openFile[ff].common.name[0]) os_printf("  open: %s\n", openFile[ff].common.name);
		}
		fileError =  F_ERR_DEL_OPENFILES;
		return false;
	}
os_printf("\n[\x1B[s\x1B[79G]\x1B[u\x1B[C");
	// open FAT
	t_FAT* fat = OpenFAT();
	if (!fat) return false;
	uint8 nfiles = fat->nfiles;

	uint8 fdel;
	// find file in FAT by name
	for (fdel=0; fdel<nfiles; fdel++)
		if (os_strncmp(name, fat->file[fdel].name, MAX_FILENAME_LEN)==0)
			break;
	if (fdel==nfiles) {
		fileError = F_ERR_FILE_NOT_FOUND;
		return false;
	};

	if (fdel==nfiles-1) {
		// simplest case: just delete file record
		fat_nfiles--;
		if (!SaveFAT(0)) return false;
		if (!SaveFAT(1)) return false;
		TryFree();
		return true;
	}

	// allocate buffers
	if (!SegmentBufferAllocate()) { fileError = F_ERR_FILE_ALLOC; return false; }

	bool fdone, done = false;
	bool tchanged = false;
	uint32 taddr, saddr;
	uint16 toffs, soffs, clen;
	uint32 fsize;
	uint8 fr = fdel+1;

	taddr = fat->file[fdel].segment * SEGMENT_SIZE + fat->file[fdel].offset;

	char* sourceBuf = NULL;
	char* targetBuf = rwSegment;
	if (!ReadSegment(taddr / SEGMENT_SIZE, targetBuf)) return false;

	while (!done) {
		//
		fdone = false;
    os_printf("  next file is \"%s\" positioned at %X:%03X, size = %u\n", fat->file[fr].name, fat->file[fr].segment, fat->file[fr].offset, fat->file[fr].size);
		fsize = fat->file[fr].size + MIME_SIZE;
		saddr = fat->file[fr].segment * SEGMENT_SIZE + fat->file[fr].offset;
		sourceBuf = GetSourceBuffer( saddr / SEGMENT_SIZE );
		if (taddr % SEGMENT_SIZE < MIME_SIZE) {
			taddr += SEGMENT_SIZE - taddr % SEGMENT_SIZE; // not enough space for mime: skip to next segment
      os_printf("taddr shift, now = %08X\n", taddr);
			ReadSegment( taddr / SEGMENT_SIZE, targetBuf );
			tchanged = false;
		}
		// modify file record
		fat->file[fr].segment = taddr / SEGMENT_SIZE;
		fat->file[fr].offset = taddr % SEGMENT_SIZE;
		while (fsize) {
			toffs = taddr % SEGMENT_SIZE;
			soffs = saddr % SEGMENT_SIZE;
			clen = _MIN( fsize, DATA_SIZE - soffs );
			clen = _MIN( clen, DATA_SIZE - toffs );
      os_printf("taddr = %08X, saddr = %08X, fsize = %u, copying %u bytes from %X:%X to %X:%X\n", taddr, saddr, fsize, clen, saddr/SEGMENT_SIZE, soffs, taddr/SEGMENT_SIZE, toffs);
			os_memcpy( targetBuf + toffs, sourceBuf + soffs, clen );
			tchanged = true;
			taddr += clen;
			saddr += clen;
			fsize -= clen;
			if (!fsize) fdone = true;
      os_printf("taddr = %08X, saddr = %08X, fsize = %u\n", taddr, saddr, fsize);
			if (toffs + clen == DATA_SIZE) {
        os_printf("writing target segment %X\n", taddr / SEGMENT_SIZE);
				WriteSegment( taddr / SEGMENT_SIZE, targetBuf );
				os_printf(".");
				taddr += CTRL_SIZE;
				if (fdone) { 
					SaveFAT(0); 
					SaveFAT(1); 
					fdone = false;
				}
        os_printf("reading target segment %X\n", taddr / SEGMENT_SIZE);
				ReadSegment( taddr / SEGMENT_SIZE, targetBuf );
				tchanged = false;
			}
			if (soffs + clen == DATA_SIZE) {
				saddr += CTRL_SIZE;
        os_printf("reading source segment %X\n", saddr / SEGMENT_SIZE);
				sourceBuf = GetSourceBuffer( saddr / SEGMENT_SIZE );
			}
		}
    os_printf("file moved\n");
		if (fr==nfiles-1) done = true; else fr++;
		if (done && tchanged) WriteSegment( taddr / SEGMENT_SIZE, targetBuf );
		if (fdone) {
			SaveFAT(0);
			SaveFAT(1);
			fdone = false;
		}  
	}

	for (fr=fdel+1; fr<nfiles; fr++)
		fat->file[fr-1] = fat->file[fr];
	fat_nfiles--;
	SaveFAT(0);
	SaveFAT(1);

	DeleteSourceBuffer();
	SegmentBufferFree();
	TryFree();

	return true;
}

// ============================================================================
// Read file
//
//   Read file from current position into {buf} and move position ahead to {len} bytes.
//
//  In: f    : file handle
//      buf  : read buffer
//      len  : number of bytes to read
//
// Out: number of bytes actually read / -1 for error
//
sint32 FLASH_CODE KFS_Read (t_File *f, char* buf, uint32 len)
{
	return KFS_RW(f, buf, len, 0);
}

// ============================================================================
// Write file
//
//   Write to file into current position from {buf} and move position ahead to {len} bytes.
//   If file is { overwritten or newly created } with filesize == 0 then it is expandable
//   which means that its size is not defined until it is closed, i.e. file grows on writes.
//
//  In: f    : file handle
//      buf  : data to write
//      len  : number of bytes to write
//
// Out: number of bytes actually written / -1 for error
//
sint32 FLASH_CODE KFS_Write (t_File *f, char *buf, uint32 len)
{
	return KFS_RW(f, buf, len, 1);
}
