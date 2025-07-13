
#include "knopa_json.h"

#define ERR_JSON_OK              0
#define ERR_JSON_BAD_SCHEME      1
#define ERR_JSON_ARRAY_EXPECTED  2
#define ERR_JSON_OBJECT_EXPECTED 3
#define ERR_JSON_VALUE_EXPECTED  4

static char* jsonErrorMsg[] = {
	"",
	"Bad scheme",
	"Array expected",
	"Object expected",
	"Value expected"
};

//static void printn(char* p, int n) { while (*p && n--) os_printf("%c", *p++); if (*p) os_printf("[...]"); };

static char* FLASH_CODE Skip(char *p)
{
	while (*p==' ' || *p=='\t' || *p=='\n' || *p=='\r') p++;
	return p;
}

// store value at {p} into field {d+off} using definition {def}, with respect to struct length {lstruct}
static char* FLASH_CODE Store(char* p, char* def, void* d, uint16* off, uint16 lstruct)
{
//os_printf("-+-+-+-+ [mem = %d] Store(p=", system_get_free_heap_size());
//printn(p,30);
//os_printf(", def=%s, d=%X, *off=%d, lstruct=%d)\n", def, d, *off, lstruct);
	p = Skip(p);
//printn(p,30);
//os_printf("\n");
	if (*def=='[' || *def=='{') p = ParseJson(p, (char*)d+(*off), def);
	else if (*def=='b' || *def=='w' || *def=='d') {
		p = GetInteger(p, (char*)d+(*off), *def);
	}
	else if (*def=='s') {
//		os_printf("p1=");
//		printn(p,30);
//		os_printf("\n");
		p = GetString(p, (char*)d+(*off)); 
//		os_printf("p2=");
//		printn(p,30);
//		os_printf("\n");
	}
	else if (*def=='n') { 
		p = GetString(p, (char*)d+(*off)); 
//		os_printf("p=");
//		printn(p,30);
//		os_printf("\nd:%s.\n", (char*)d+(*off));
		char *pp = kstrcat((char*)d+(*off),"\n");
		*off = pp - ((char*)d+(*off)); 
//		os_printf("off=%d\n", *off);
	}
	return p;
}

static inline char* SeekAfter (char* p, char ch)
{
	while (*p && *p!=ch) p++;
	if (*p==ch) return p+1;
	else return NULL;
}

static inline bool ProbeTo (char* p, char ch)
{
	while (*p && *p!=ch) p++;
	return *p==ch;
}

static char* SkipString (char* p) 
{
	bool esc = false;
	while (*p) {
		if (*p=='\\') esc = true;
		else if (*p=='"' && !esc) break;
		p++;
		esc = false;
	}
	if (*p=='"') p++;
	return p;
}
static char* SkipValue (char* p);
static char* SkipObject (char* p)
{
	while (*p) {
		p = Skip(p);
		if (*p=='}') return p+1;
		if (*p==',') p++;
		p = SeekAfter(p,':');
		p = SkipValue(p);
	}
}
static char* SkipArray (char* p)
{
	while (*p) {
		p = Skip(p);
		if (*p==']') return p+1;
		p = SkipValue(p);
	}
}
static char* SkipValue (char* p)
{
	while (*p && *p!='"' && *p!='{' && *p!='[' && !(*p>='0' && *p<='9')) p++;
	if (*p==0) return p;
	if (*p=='"') p = SkipString(p+1);
	else if (*p=='{') p = SkipObject(p+1);
	else if (*p=='[') p = SkipArray(p+1);
	else while (1) { if (!((*p>='0' && *p<='9') || *p=='-' || *p=='.')) break; p++; }
	p = Skip(p);
	if (*p==',') p++;
	return p;
}

// ============================================================================
// Get last error message
//
static char* pjson;
static char* pjerr;
static int  jError;
static char jErrorMsg[32];
char* FLASH_CODE JsonError (void)
{
	char *p = jErrorMsg;
	p[0] = 0;
	if (jError > 0) p += os_sprintf(p, "%s", jsonErrorMsg[jError]);
	if (jError > 1) p += os_sprintf(p, " at offset %d", pjerr - pjson);
	return jErrorMsg;
}

// ============================================================================
// Parse JSON string 
//
//
// Ex: ParseJson ('[{"cid":5, "ival":["123","456"]},{"cid":9, "ival":["Bebebe"]}]', &data, '[4{50|cid/0,ival/2|b,[4|8|s]}]')
//
static int jentry = 0;
char* FLASH_CODE ParseJson (char* json, void* data, char* scheme)
{
//os_printf("ParseJson(json=%s, data=%X, scheme=%s)\n", json, data, scheme);
	if (jentry==0) pjson = json;
	char *pj;
	const char PIPE = '|';
	if (!jentry) jError = ERR_JSON_OK; else jentry++;
	pjson = json;
	uint8 nfld = 0; 
	uint16 lstruct = 0;
	uint16 maxarr = 1;
	char fld[16][16];
	uint16 fldoff[16];
	char* flddef[16];
	char* s =  scheme;
	char* d;
	// parse scheme -----------------------------------------
	bool arr = (*s=='[');
	if (arr) { s++; s = GetInteger(s, &maxarr, 'w'); } // max array size
	bool obj = (*s=='{');
	if (*s!='{' && *s!=PIPE) { jError = ERR_JSON_BAD_SCHEME; jentry--; return NULL; }
	s++; // '{' or '|'
	s = GetInteger(s, &lstruct, 'w'); // object length (0 for CR-delimited strings)
	if (!arr && !lstruct || *s!=PIPE) { jError = ERR_JSON_BAD_SCHEME; jentry--; return NULL; }
	s++; // '|'
	// fields
	if (!obj) nfld = 1; // value array
	fld[0][0] = 0;
	fldoff[0] = 0;
	while (obj && *s && *s!=PIPE) {
		// field names / offsets
		d = fld[nfld];
		while (*s && *s!=',' && *s!='/' && *s!=PIPE) *d++ = *s++;	*d = 0;
		if (*s=='/') s = GetInteger(s+1, &fldoff[nfld], 'w');
		if (*s==',') s++;
		nfld++;
	}
//uint16 ii;
//for (ii=0; ii<nfld; ii++) os_printf("  fld[%d]=\"%s\" @ %u\n", ii, fld[ii], fldoff[ii]);
	if (*s==PIPE) s++; // '|'
	// field types
	uint8 i = 0;
	while (*s && *s!='}' && *s!=']') {
		flddef[i] = s;
		uint8 brsq=0, brcr=0;
		while (*s && !((*s=='}' || *s==']' || *s==',') && brsq==0 && brcr==0)) {
			if (*s=='[') brsq++;
			if (*s==']') brsq--;
			if (*s=='{') brcr++;
			if (*s=='}') brcr--;
			s++;
		}
		if (*s==',') s++;
		i++;
	}
	if (i!=nfld) { jError = ERR_JSON_BAD_SCHEME; jentry--; return NULL; }
//for (ii=0; ii<nfld; ii++) os_printf("  flddef[%d]=%s\n", ii, flddef[ii]);

	pj = Skip(json);
	d = data;
	if (arr) {
		pj = SeekAfter(pj,'[');
		if (!pj) { pjerr = pj; jError = ERR_JSON_ARRAY_EXPECTED; jentry--; return NULL; }
	}

	// parse json ------------------------------------------
	int narr = 0;
	uint16 off;
	while (1) {
		// scanning list of array elements OR object key-value pairs
		off = 0;
		if (obj) {
			// object
			pj = SeekAfter(pj,'{');
			if (!pj) { pjerr = pj; jError = ERR_JSON_OBJECT_EXPECTED; jentry--; return NULL; }
			while (*pj && *pj!='}') {
				// walking through object's key-value pairs
				char field[16];
//				os_printf("pj=");
//				printn(pj,30);
//				os_printf("\n");
				pj = SeekAfter(GetString(pj,field),':');
//				os_printf("field=\"%s\"\n", field);
				if (!pj) { pjerr = pj; jError = ERR_JSON_VALUE_EXPECTED; jentry--; return NULL; }
				bool found = false;
				for (i=0; i<nfld; i++) {
					if (os_strcmp(field,fld[i])==0) {
						found = true;
						// field definition found: store value
						pj = Store(pj, flddef[i], d, &fldoff[i], lstruct);
						if (!pj) return NULL;
						break;
					}
				} // for nfld
				if (!found) {
//					os_printf("%s not found, skipping at ",field);
//					printn(pj,30);
//					os_printf("\n");
					pj = SkipValue(pj);
//					os_printf("skipped=");
//					printn(pj,30);
//					os_printf("\n");
				}
			}
			if (*pj=='}') pj++;
		} else { // if obj
			// array of ints/strings
			pj = Store(pj, flddef[0], d, &off, lstruct);
			if (!pj) return NULL;
		}		
		pj = Skip(pj);
		if (*pj!=',') break; else pj++;
		pj = Skip(pj);
		d += lstruct ? lstruct : off;
		if (++narr >= maxarr) break;
	} // while (1)

	if (arr && *pj==']')  pj++;
	
	jentry--;
	return pj;
}
