
#include "knopa_string.h"

// ============================================================================
// Concat {src} to the end of {dest}, returl pointer to Z in {dest}
//
char* FLASH_CODE kstrcat(char* dest, char* src)
{
	while (*dest) dest++;
	while (*dest++ = *src++) ;
	return dest-1;
}
// ============================================================================
// Copy from {p} to {d} until EOL or {comma} or {term} encountered or {maxlen} chars copied.
// Skip {comma} if {d} is terminated by it.
//
// Return {p} which points to {term} OR next char after {comma} (whichever found 1st)
//
// Note: {maxlen} is a total length of output buffer, including trailing zero.
//			 {maxlen} = 0 means no limits
//
char* FLASH_CODE CopyToTerm(char* d, char* p, char comma, char term, uint16 maxlen)
{
	char pct[3],pctn=0;
	uint16 done=0;
	*pct = 0;

	while (*p && *p!='\n' && *p!=comma && *p!=term && maxlen) {
		if (*p=='+') { maxlen--; *d++ = ' '; }
		else if (*p=='%') (*pct)++;
		else if (*pct) pct[(*pct)++] = *p;
		else { maxlen--; *d++ = *p; }
		p++;
		if (*pct==3) {
			if (pct[1]>='0' && pct[1]<='9') pct[1]-='0';
			else if (pct[1]>='A' && pct[1]<='F') pct[1]-='A'-10;
			if (pct[2]>='0' && pct[2]<='9') pct[2]-='0';
			else if (pct[2]>='A' && pct[2]<='F') pct[2]-='A'-10;
			*d++ = (pct[1]<<4)+pct[2];
			maxlen--;
			*pct = 0;
		}
	}
	*d = 0;
	if (*p==comma) p++;
	return p;
}

// ============================================================================
// Parse json-escaped unicode character
//
LOCAL char* FLASH_CODE ParseUnicode (char** val, char* p)
{
	char* v = *val;
	if (*p=='u') p++;
	int i, code = 0;
	for (i=0; i<4; i++)
	*val = v;
	return p;
}

// ============================================================================
// Get unsigned integer value from string {p} of type {type}
//
char* FLASH_CODE GetInteger (char* p, void* val, char type)
{
	uint8  *pb = (uint8*)val;
	uint16 *pw = (uint16*)val;
	uint32 *pd = (uint32*)val;
	uint32 i = 0;

	while (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r') p++;
	while (*p>='0' && *p<='9') {
		i = i*10 + (*p)-'0';
		p++;
	}
	if (type=='b') *pb = (uint8)i;
	if (type=='w') *pw = (uint16)i;
	if (type=='d') *pd = (uint32)i;
	return p;
}

// ============================================================================
// Get quoted string
//
// Returns pointer to the next character after trailing quote
//
char* FLASH_CODE GetString (char* p, char* d)
{
	*d = 0;
	while (*p && *p!='"')  p++;
	if (*p==0) return p;
	p++; // "
	while(*p && *p!='"') {
		if (*p=='\\') {
			p++;
			if (*p=='u') p = ParseUnicode(&d, p);
			else *d++ = *p++;
		} 
		else *d++ = *p++;
	}		
	*d = 0;
	while (*p=='"') p++;
	return p;
}

// ============================================================================
// Case-insensitive comparision of ASCIIZ strings, with length limitation
//
sint16 FLASH_CODE stricmpn (char *a, char *b, uint16 len)
{
	signed char ca, cb, d;
	while (*a && *b && len--) {
		ca = *a++;
		cb = *b++;
		if (ca>='a' && ca<='z') ca -= 'a'-'A';
		if (cb>='a' && cb<='z') cb -= 'a'-'A';
		d = ca-cb;
		if (d) return d;
	}
	return 0;
}

// ============================================================================
// Case-sensitie substring search, with length limitation
//
sint32 FLASH_CODE strstrn (char *haystack, const char *needle, uint32 len)
{
	char* begin = haystack;
	while (*haystack && len--) {
		char* p = haystack;
		const char* d = needle;
		uint16 ln = len;
		while (*p==*d && ln--) { p++; d++; }
		if (*d==0) return haystack-begin;
		haystack++;
	}
	return -1;
}

// ============================================================================
// Convert string {s} into signed int32.
// Skip leading spaces and parse integer until first non-digit character encountered
//
// Return sint32
//
sint32 FLASH_CODE atoi (char* s)
{
	if (!s) return 0;

	sint32 i = 0;
	uint8 sign = 0;
	while (*s==' ') s++;
	while ((*s>='0' && *s<='9') || *s=='-') {
		char ch = *s++;
		if (ch=='-' && i==0) sign = 1;
		if (ch>='0' && ch<='9') i = i*10+(ch-'0');
	}
	return sign ? -i : i;
}

// ============================================================================
// Convert string {s} into signed int32.
// Skip leading spaces and parse integer until first non-digit character encountered
//
// Return sint32
//
sint32 FLASH_CODE atoin (char** s)
{
	if (!s) return 0;
	if (!*s) return 0;

	char *p = *s;

	sint32 i = 0;
	uint8 sign = 0;
	while (*p==' ') p++;
	while ((*p>='0' && *p<='9') || *p=='-') {
		char ch = *p++;
		if (ch=='-' && i==0) sign = 1;
		if (ch>='0' && ch<='9') i = i*10+(ch-'0');
	}
	while (*p && !((*p>='0' && *p<='9') || *p=='-')) p++;

	*s = p;

	return sign ? -i : i;
}

// ============================================================================
// Count \n-delimited strings in {source}
// 
uint16 FLASH_CODE CountCRDelimited(char* source)
{
	uint16 n = 0;
	while (*source) {
		while (*source && *source!='\n') source++;
		if (*source=='\n') { n++; source++; }
	}
	return n;
}

// ============================================================================
// Read \n-delimited strings {source} and copy those into variable-size indexed asciiz string array {dest}
// First {n} bytes of {dest} point to corresponding strings.
// 
// Ex: CRDelimited( dest, "One\nTwo\n\nFour", 4 ) --> dest = [ 4, 8, 12, 14, "One", "Two", "", "Four" ]
//     so: dest+dest[0] = "One"
//         dest+dest[1] = "Two"
//         dest+dest[2] = ""
//         dest+dest[3] = "Four"
//
void FLASH_CODE CRDelimited(char* dest, char* source, int n)
{
	char* dbase = dest;
	char* ibase;
	uint32 i;

	dest += n;
	ibase = dest;
	for (i=0; i<n; i++) {

		while (*source && *source!='\n') *dest++ = *source++;
		if (*source) source++; // skip \n
		dbase[i] = ibase-dbase;
		*dest++ = 0;
		ibase = dest;
	}
}

// ============================================================================
// Find {param} in zero-delimited strings array {conn->params}
//
char* FLASH_CODE GetParameterValue (char* params, int paramCount, char* param)
{
	int i;
	char *v;
	for (i=0; i < paramCount; i++) {
		v = params; while (*v++) ;
		if (os_strcmp(params, param)==0) {
			return v;
		}
		params = v; while (*params++) ;
	}

	return NULL;
}

// ============================================================================
// Calculate CRC32 for given memory area
// Credits: http://www.hackersdelight.org/hdcodetxt/crc.c.txt
//
// Out:  32-bit CRC
//
uint32 FLASH_CODE crc32c (char *data, uint16 length)
{
	uint16 byte, i;
	uint32 crc, mask;
	static uint32 crc_table[256] = {0,0};

	// Set up the table if necessary

	if (crc_table[1]==0) {
		for (byte = 0; byte < 256; byte++) {
			crc = byte;
			for (i=0; i<8; i++) { 	 // Do eight times.
				mask = -(crc & 1);
				crc = (crc >> 1) ^ (0xEDB88320 & mask);
			}
			crc_table[byte] = crc;
		}
	}

	// Through with table setup, now calculate the CRC

	crc = 0xFFFFFFFF;
	while (length--) {
		byte = *data++;
		crc = (crc >> 8) ^ crc_table[(crc ^ byte) & 0xFF];
	}
	return ~crc;
}
