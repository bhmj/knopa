#ifndef _KNOPA_STRING_H
#define _KNOPA_STRING_H

#include <osapi.h> // os_strcmp
#include "kmem.h"

// ============================================================================
// Concat {src} to the end of {dest}, returl pointer to Z in {dest}
//
char* FLASH_CODE kstrcat(char* dest, char* src);

// ============================================================================
// Copy from {p} to {d} until EOL or {comma} or {term} encountered or {maxlen} chars copied.
// Skip {comma} if {d} is terminated by it.
//
// Return {p} which points to {term} OR next char after {comma} (whichever found 1st)
//
// Note: {maxlen} is a total length of output buffer, including trailing zero.
//			 {maxlen} = 0 means no limits
//
char* FLASH_CODE CopyToTerm(char* d, char* p, char comma, char term, uint16 maxlen);

// ============================================================================
// Get unsigned integer value from string {p} of type {type}
//
char* FLASH_CODE GetInteger (char* p, void* val, char type);

// ============================================================================
// Get quoted string
//
char* FLASH_CODE GetString (char* p, char* d);

// ============================================================================
//	Case-insensitive comparision of ASCIIZ strings, with length limitation
//
sint16 FLASH_CODE stricmpn (char *a, char *b, uint16 len);

// ============================================================================
//	Case-sensitie substring search, with length limitation
//
sint32 FLASH_CODE strstrn (char *haystack, const char *needle, uint32 len);

// ============================================================================
// Convert string {s} into signed int32.
// Skip leading spaces and parse integer until first non-digit character encountered
//
// Return sint32
//
sint32 FLASH_CODE atoi (char* s);

// ============================================================================
// Count \n-delimited strings in {source}
// 
uint16 FLASH_CODE CountCRDelimited(char* source);

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
void FLASH_CODE CRDelimited(char* dest, char* source, int n);

// ============================================================================
// Find {param} in zero-delimited strings array {conn->params}
//
char* FLASH_CODE GetParameterValue (char* params, int paramCount, char* param);

// ============================================================================
// Calculate CRC32 for given memory area
// Credits: http://www.hackersdelight.org/hdcodetxt/crc.c.txt
//
// Out:  32-bit CRC
//
uint32 FLASH_CODE crc32c (char *data, uint16 length);

#endif
