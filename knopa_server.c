#include <osapi.h>
#include <c_types.h>
#include <ip_addr.h>
#include <espconn.h>
#include <user_interface.h>
//#include <mem.h>

#include "common.h"
#include "knopa_server.h"
//#include "knopa_json.h"
#include "knopa_misc.h"
#include "knopa_dbg.h"
#include "knopa_fs.h"
#include "knopa_sys.h"
#include "knopa_string.h"

#include "files.html.h"

#define SERVER_NAME "KnopaServer"
#define SERVER_VERSION "0.33"

#define SERVER_PORT 80
#define SERVER_SSL_PORT 443

typedef enum {
	SEND_NOT_COMPLETED,
	SEND_LAST_CHUNK,
	SEND_COMPLETED
} enDataSent;

typedef enum {
	CONTENT_NONE, 							 // no Content-Type specified in request
	CONTENT_MULTIPART,					 // multipart/form-data (POST ONLY)
	CONTENT_FORM, 							 // application/x-www-form-urlencoded
	CONTENT_OTHER 							 // other
} enContentType;

typedef struct {
	// do not move or modify this section as it mirrors t_HttpOut!
	char* output; 							 // output buffer
	uint32 outputLength;				 // length of allocated buffer
	uint32 outputData;					 // length of data to send
	uint32 contentLength; 			 //
	uint32 contentSent; 				 // works together with contentLength
	uint16 paramCount;					 // urlencoded param count
	char *param;								 // dynamically allocated urlencoded params buffer (asiizz)
	// general
	struct espconn *pespconn; 	 // related esp connection
	uint32 remote_ip4;					 //
	uint32 remote_port; 				 //
	// incoming
	enContentType contentType;	 // content type in request
	uint32 contentReceived; 		 // progress: uploading long data
	enRequestMethod method; 		 // get or post
	char path[64];							 // requested resourse
	uint16 valueCount;					 // urlencoded value count
	uint32 paramPos;						 // write cursor position
	char boundary[70];					 // boundary for multipart/form-data
	uint8 boundaryLength; 			 // boundary length
	char pct[5];								 // partial percent-encoded character
	uint8 pctn; 								 // pct length
	// file uploading
	char* preload;							 // intermediate buffer for multipart data chunks
	uint32 preloadLength; 			 //
	// file operations (I/O)
	uint16 nFileHandle;          // number of file handles
	uint32 fileHandle[4];        // file handle >0 ? sending file data
	// outgoing
	char* headers;							 //
	uint8 headerSent; 					 // HTTP header has been sent
	uint8 completed;						 // 0 = data has not been completely sent, 1 = this is a last chunk of data, 2 = all outgoing data has been sent (server can close connection)
	t_fn_DataProvider dataProvider; // data provider function
} t_Connection;

// ============================================================================
// VARS
#define MAX_CONN 4

// connections
static t_Connection connData[MAX_CONN];

// ============================================================================
// VARS
#define SRV_EXT_NONE					 0
#define SRV_EXT_MEMORY				 1
#define SRV_EXT_FILE_OPEN 		 2
#define SRV_EXT_FILE_READ 		 3
#define SRV_EXT_FILE_WRITE		 4

static char* serverError[] = {
	"",
	"Not enough memory",
	"Cannot open file: ",
	"Cannot read file: ",
	"Cannot write file: "
};

// ============================================================================
// file upload interface functions
//
t_fn_ProcessRequest fn_Process;

static uint32 FLASH_CODE Upload_OpenFile	(char* fname, char* mime)
{
	// force using some important types
	char* ext = KFS_FileExtension(fname);
	if (os_strcmp(ext,"json")==0) mime = "application/json";
	t_File* f = KFS_Open(fname, "w", mime, 0);
	return (uint32)f;
}
static uint32 FLASH_CODE Upload_WriteFile (uint32 handle, char* buf, uint32 length)
{
	if (KFS_Write((t_File*)handle, buf, length) < 0) { os_printf("write error: %s\n", KFS_ErrorMessage()); return 0; }
	return 1;
}
static uint32 FLASH_CODE Upload_CloseFile (uint32 handle)
{
	KFS_Close((t_File*)handle);
	return 1;
}

// ============================================================================
// Forward declarations

void	 FLASH_CODE ServeFile (t_HttpOut* httpout);
void	 FLASH_CODE ProcessRequest (t_Connection* conn);
void	 FLASH_CODE FilesRead (t_HttpOut* httpout);
void	 FLASH_CODE FilesRename (t_Connection* conn);

// ============================================================================
// Send HTTP header
//
//	In:  conn 	 = connection
//			 headers = extra headers, \n delimited
//
//			 conn->boundary 			= content-type
//			 conn->contentLength	= content-length
//
void FLASH_CODE httpd_send_header (t_Connection *conn, char* headers)
{
	if (conn->headerSent) return;

	conn->headers = k_malloc(256, "conn->headers", "httpd_send_header");
	if (*conn->boundary==0 || *conn->boundary=='-') os_strcpy(conn->boundary, "text/html");
	os_sprintf(
		conn->headers,
		"HTTP/1.0 200 OK\r\nServer: " SERVER_NAME "/" SERVER_VERSION "\r\nContent-type: %s\r\nContent-Length: %u%s%s\r\n\r\n",
		conn->boundary, 				// content-type
		conn->contentLength,		// content-length
		headers ? "\r\n" : "",	// line feed before extra headers
		headers ? headers : ""	// extra headers
	);
	conn->headerSent = 1;
	espconn_send(conn->pespconn, conn->headers, os_strlen(conn->headers));
}

// ============================================================================
// Send HTTP error, extended version
//
//	In:  conn 	 = connection
//
void FLASH_CODE httpd_error_ext (t_Connection *conn, uint16 httpCode, uint8 serverCode, char* message)
{
	os_printf("httpd error (%u, %u, \"%s\")\n", httpCode, serverCode, message);
	int i;
	int httpCodeList[] = { 200, 400, 404, 405, 413, 418, 500, 503, 0 };
	char* httpCodeText[] = { "OK", "Bad request", "Not Found, try <a href=\"/files\">File Manager</a>", "Method not allowed", "Request Entity Too Large", "I'm a Teapot", "Internal Server Error", "Service Unavailable", "Unknown error" };
	char* htmlWrapper = "<html><head><meta name=viewport content=\"width=device-width, initial-scale=1\"></head><body><h2>%s</h2><h2>%s</h2><h3>%s</h3></body></html>";

	if (conn->completed==SEND_COMPLETED) return;

	conn->headers = k_malloc(512, "conn->headers", "httpd_error_ext");

	for (i=0; httpCodeList[i]; i++)
		if (httpCodeList[i]==httpCode) break;

	int clen = os_strlen(htmlWrapper)-6 + os_strlen(httpCodeText[i]) + os_strlen(serverError[serverCode]) + (message?os_strlen(message):0);
	int headlen = os_sprintf(
		conn->headers,
		"HTTP/1.0 %u %s\r\nServer: KnopaHttpd/0.33\r\nContent-Type: text/html\r\nContent-Length: %u\r\n\r\n",
		httpCode,
		httpCodeText[i],
		clen
	);
	os_sprintf(
		conn->headers + headlen,
		htmlWrapper,
		httpCodeText[i],
		serverError[serverCode],
		message?message:""
	);
	conn->completed = SEND_COMPLETED;
	espconn_send(conn->pespconn, conn->headers, os_strlen(conn->headers));
}

void FLASH_CODE httpd_error (t_Connection *conn, uint16 httpCode)
{
	os_printf("httpd_error(%u)\n", httpCode);
	httpd_error_ext(conn, httpCode, SRV_EXT_NONE, NULL);
}


void FLASH_CODE HttpError (t_HttpOut *httpout, uint16 httpCode)
{
	os_printf("httpd_error(%u)\n", httpCode);
	httpd_error_ext((t_Connection*)httpout, httpCode, SRV_EXT_NONE, NULL);
}

// ============================================================================
// Scan {p} for {header} stopping at \0 or {eob} or \r\n\r\n reached
// Copy header value into {value}
// If {attrib} is passed, also scan for {attrib} of the specified {header}, copy to {avalue}
//
//				p : source buffer
//	 header : header to find
//			eob : end-of-buffer address
//		value : value buffer
//	 attrib : attrib to find (note: include '=' in attrib name, like so: "filename=")
//	 avalue : attrib buffer
//
// Return:
//				0 : {eob} encountered
//				1 : done ok
//
uint8 FLASH_CODE GetHttpHeaderValue(char* p, char* header, char* eob, char* value, char* attrib, char* avalue, int avalue_len)
{
	uint16 len = os_strlen(header);
	uint16 alen = attrib ? os_strlen(attrib) : 0;
	uint16 written = 0;

	*value = 0;
	if (avalue) *avalue = 0;

	while (1) {
		if (stricmpn(p, header, len) == 0) {
			// we found header at new line
			p += len;
			while (*p && p<eob && (*p==':' || *p==' ' || *p=='\t')) p++; // skip ':' and spaces
			while (*p && p<eob && *p!=';' && *p!='\r') *value++ = *p++;
			*value = 0; // close asciiz
			if (attrib && avalue) {
				char* av = avalue;
				while (*p && p<eob && *p!='\r' && stricmpn(p,attrib,alen)) p++;
				if (stricmpn(p,attrib,alen)==0) {
					p += alen;
					if (*p=='"') p++;
					while (*p && p<eob && *p!='"' && *p!=';' && *p!='\r' && ++written < avalue_len) *avalue++ = *p++; // copy alue, quoted or not
					*avalue = 0; // close asciiz
				}
				while (*p && p<eob && *p!='\r') p++; // seek to EOL
			}
			// if we finished parsing up to EOL -- success
			if (*p=='\r' && *(p+1)=='\n' && p < eob-1) return 1; else return 0;
		} else {
			// skip to next line
			while (1) {
				while (*p && p<eob && *(p++)!='\n') ;
				if (p==eob || (*p!=' ' && *p!='\t')) break;
			}
			if (*p=='\r' && *(p+1)=='\n') return 1; // not found, but it's ok
			if (*p==0 || p==eob) return 0;					 //
		}
	}
	return 0;
}

// ============================================================================
// Copy and parse HTTP parameters (GET/POST)
//
// Allocate memory for parameters buffer if needed
//
// Read parameters from {p} to allocated buffer, decoding as needed, stopping at <space>, "\r\n" or {eob}
//
// Result: {conn->param} as \0-delimited ASCIIZ string array containing "param":"value" pairs
//
// Return: 0 : data processed, not finished +
//				 1 : data processed, finished +
//				 2 : memory allocation error +
//
uint8 FLASH_CODE ParseHttpParams (t_Connection *conn, char *p, char *eob)
{
	uint32 sz = eob - p;
	char *d, *pct;

	if (!conn->param) {
		// first time
		if (conn->contentLength) sz = conn->contentLength; else conn->contentLength = sz;
		conn->param = k_malloc(sz + 128, "conn->param", "ParseHttpParams"); // reserve some bytes for "param&param=val" causes
		if (!conn->param) return 2; // memory allocation error
		conn->paramPos = 0;
		conn->paramCount = 0;
		conn->valueCount = 0;
		conn->contentReceived = 0;
	} else {
		// continued
		if (sz > conn->contentLength+128) {
			d = (char*)os_realloc(conn->param, conn->contentLength + sz + 128);
			if (!d) {
				k_free(conn->param);
				conn->param = NULL;
				return 2; // memory allocation error
			};
			conn->param = d;
		}
	}
	d = conn->param + conn->paramPos;
	pct = conn->pct; // pct = [Nccc], where N is substring length, ccc is substring

	uint8 done = 0;

	while (*p && p<eob) {
		if (*pct) { // continue pct
			pct[(*pct)++] = *p;
		}
		else if (*p=='%') (*pct)++; 	// percent
		else if (*p=='+') *d++ = ' '; // space
		else if (*p=='=') { *d++ = 0; conn->paramCount++; } // parameter completed
		else if (*p=='&') { *d++ = 0; conn->valueCount++; if (conn->paramCount < conn->valueCount) {conn->paramCount++; *d++ = 0; } } // value completed
		else *d++ = *p;

		if (*pct==3) {
			// percent-coded byte complete: decode
			if (pct[1]>='0' && pct[1]<='9') pct[1]-='0';
			else if (pct[1]>='A' && pct[1]<='F') pct[1]-='A'-10;
			if (pct[2]>='0' && pct[2]<='9') pct[2]-='0';
			else if (pct[2]>='A' && pct[2]<='F') pct[2]-='A'-10;
			*d++ = (pct[1]<<4)+pct[2];
			*pct = 0;
		}
		p++;
		conn->contentReceived++;
		if (*p==' ' || *p=='\t' || (*p=='\r' && *(p+1)=='\n') || conn->contentReceived == conn->contentLength) { done++; break; }
	};

	if (done) {
		*d++ = 0; // finish last element
		conn->valueCount++; // supposed to be a value
		if (conn->paramCount < conn->valueCount) {conn->paramCount++; *d++ = 0; } // was parameter? zero the value
		*d = 0; // asciizz
	}
	conn->paramPos = d - conn->param;

	return done;
}

// ============================================================================
// Parse multipart/data (POST)
//
// Scan input data {p} for {conn->bound}, saving files as needed. Stop at {eob} or {contentLength} read.
//
// Return: 0 : data processed, not finished +
//				 1 : data processed, finished +
//				 2 : memory allocation error +
//				 3 : data consistency error +
//				 4 : file open error +
//				 5 : file write error +
//
char crcn(char a) { if (a=='\r') return 'r'; if (a=='\n') return 'n'; return a; }

uint8 FLASH_CODE ParseMultipartData (t_Connection *conn, char *p, char *eob)
{
	char *pct = conn->pct;
	uint8 *pctn = &conn->pctn;
	uint8 lim, pctnc = *pctn; // pctnc: pct counter from last entry
	sint32 marked = -1;
	char* buf = NULL;
	char* xp;
	char header[80], fname[80];
	int i;

	conn->contentReceived += eob - p;
	// in case we have already registered error
	if (conn->output) return conn->contentReceived < conn->contentLength ? 0 : 1;

	if (conn->preload) {
		pct[4] = 0;
		// seek to end of http headers
		xp = p;
		while (xp < eob-3 && !(*xp=='\r' && *(xp+1)=='\n' && *(xp+2)=='\r' && *(xp+3)=='\n')) xp++;
		if (xp == eob-3) return 3; // data consistency: HTTP header too large?
		os_realloc(conn->preload, conn->preloadLength + (xp-p) + 4);
		if (!conn->preload) return 2; // memory allocation error
		// complete loading header
		os_memcpy(conn->preload + conn->preloadLength, p, (buf-p)+4);
		// now scan for specific header values:
		i = GetHttpHeaderValue(p+1, "Content-Disposition", eob, header, "filename=", fname, sizeof(fname));
		if (!i) return 3; // data consistency: filename not found
		i = GetHttpHeaderValue(p+1, "Content-Type", eob, header, NULL, NULL, 0);
		if (!i) return 3; // data consistency: content-type not found
		k_free(conn->preload);
		conn->preload = NULL;
		p = xp + 4; // seek to data
		conn->fileHandle[0] = Upload_OpenFile(fname, header);
		if (!conn->fileHandle[0]) return 4; // error opening file
	}

	if (conn->fileHandle[0]) { buf = p; marked = 0; }

	while (p < eob) {
		if (*pctn) {
			// collected some special symbols: check them
			i = 0;
			if (os_strncmp(pct,"\r\n--",*pctn)==0) i = 1; 		// boundary tag: tail
			else if (os_strncmp(pct,"?--",*pctn)==0) i = 2; 	// boundary tag: file end tag
			else if (os_strncmp(pct,"--",*pctn)==0) i = 3;		// boundary tag: start?
			else {
				if (pctnc && conn->fileHandle[0]) {
					// part of boundary tag the last entry, not a boudary though
					// just dump it into the file
					if (!Upload_WriteFile( conn->fileHandle[0], pct, pctnc )) return 5;
					*pctn -= pctnc;
					pctnc = 0; // forget about last entrry
				}
				if (marked >= 0) marked += *pctn;
				*pctn = 0;
				pct[4] = 0;
			}
			if ((i==1 && *pctn==4) || (i==3 && *pctn==2)) { // now look for boundary
				lim = _MIN(eob-p, conn->boundaryLength - pct[4]);
				if (os_strncmp(p, conn->boundary + pct[4], lim)==0) {
					pct[4] += lim;
					p += lim; // skip to end of boundary
					if (p==eob) break; // !
				} else {
					if (pctnc && conn->fileHandle[0]) {
						// part of boundary tag from the last entry, not a boudary though
						// just dump it into the file
						if (!Upload_WriteFile( conn->fileHandle[0], pct, pctnc )) return 5;
						*pctn -= pctnc;
						pctnc = 0; // forget about last entrry
					}
					if (marked >= 0) marked += *pctn;
					*pctn = 0;
					pct[4] = 0;
				}
			}
			if (i==2 && *pctn==3) return 1; // end of transfer
			//
			if (pct[4] == conn->boundaryLength) {
				pct[4] = 0;
				// we found boundary
				if (conn->fileHandle[0]) {
					// flush buffer and save
					if (marked) {
						if (!Upload_WriteFile( conn->fileHandle[0], buf, marked )) { os_printf("file write error");	return 5; }
					}
					Upload_CloseFile( conn->fileHandle[0] );
					conn->fileHandle[0] = 0;
					marked = -1;
				}
				//
				if (p < eob && *p=='-') { return 1; } // end of transfer
				else if (p==eob) {
					pct[0] = '?';
					*pctn = 1; // check for final boundary
				}
				//
				xp = p;
				while (xp < eob-3 && !(*xp=='\r' && *(xp+1)=='\n' && *(xp+2)=='\r' && *(xp+3)=='\n')) xp++;
				if (xp==eob-3) {
					// headers not fully loaded
					conn->preloadLength = eob-p;
					conn->preload = k_malloc(eob-p, "conn->preload", "ParseMultipartData");
					if (!conn->preload) { os_printf("malloc error"); return 2; } // memory allocation error
					os_memcpy(conn->preload, p, conn->preloadLength);
					p = eob; // jump to eob
				} else {
					i = GetHttpHeaderValue(p, "Content-Disposition", eob, header, "filename=", fname, sizeof(fname));
					if (!i) return 3; // data consistency
					i = GetHttpHeaderValue(p, "Content-Type", eob, header, NULL, NULL, 0);
					if (!i) return 3; // data consistency
					p = xp + 4; // seek to data
					conn->fileHandle[0] = Upload_OpenFile(fname, header);
					if (!conn->fileHandle[0]) { os_printf("file create error:%s\n", KFS_ErrorMessage()); return 4; } // file open error
					buf = p;
					marked = 0;
					*pctn = 0;
				}
			} // boundary
		} // checeking special symbols

		// special cases begin with special symbols:
		if (*p=='\r' || *p=='\n' || *p=='-') pct[(*pctn)++] = *p;
		else {
			if (pctnc && conn->fileHandle[0]) {
				// newly received character is not of special group and we have 
				// part of boundary tag from the last entry, not a boudary though ('cause newly received character is not special)
				// just dump it into the file
				if (!Upload_WriteFile( conn->fileHandle[0], pct, pctnc )) return 5;
				*pctn -= pctnc;
				pctnc = 0;
			}
			if (marked >= 0) marked++;
			if (*pctn > 0) { marked += *pctn; *pctn = 0; }
		}

		p++;

	} // while

	// flush current buffer
	if (conn->fileHandle[0] && marked) {
		if (!Upload_WriteFile( conn->fileHandle[0], buf, marked )) return 5;
	}

	return 0;
}

// ============================================================================
// Send detailed error message
//
//	In:  t_Connection* conn  : connection
//	     code  : error code from ParseMultipartData 
//               1 = completed
//               2 = memory allocation error
//               3 = bad request
//               4 = file open error
//               5 = file write error
//
void FLASH_CODE MultipartDataError (t_Connection* conn, uint8 code)
{
	char err[128] = "";
	if (!conn->output) {
		conn->outputLength = 256;
		conn->output = k_malloc(conn->outputLength, "conn->output", "MultipartDataError");
		char* msg[] = { "", "", "Not enough memory", "Bad request", "File open error: ", "File write error: " };
		if (code==4 || code==5) os_strcpy(err, KFS_ErrorMessage());
		os_sprintf(conn->output, "{\"result\":\"%s%s\"}", msg[code], err );
		os_strcpy(conn->boundary,"application/json");
	}
	if (code==1) HttpSend( (t_HttpOut*)conn );
}

// ============================================================================
// Parse and process HTTP request
//
//	In:  char* request  : request string 		 (ex: GET /?method=1&param=str HTTP/1.1\n .... )
//       t_Connection* conn  : connection
//
//	Out: none
//
void FLASH_CODE ParseHttpRequest (t_Connection* conn, char* request, uint16 length)
{
	char *p = request, *eob = p+length;
	char buffer[80]; // method / header value

	// method
	if (conn->method==REQUEST_UNKNOWN) {
		p = CopyToTerm(buffer, request, ' ', ' ', 16);
		if (os_strcmp(buffer,"GET")==0)  conn->method = REQUEST_GET;	// only GET and POST methods are supported
		if (os_strcmp(buffer,"POST")==0) conn->method = REQUEST_POST; //
		if (conn->method==REQUEST_UNKNOWN) { httpd_error(conn,405); return; } // Method Not Allowed
		// path
		p = CopyToTerm(conn->path, p, '?', ' ', 64);
		// url-encoded data?
		if (conn->method == REQUEST_GET && *(p-1)=='?') conn->contentType = CONTENT_FORM; // get request is treated as urlencoded form
		else conn->contentType = CONTENT_NONE;
		// read extra headers for POST:
		if (conn->method==REQUEST_POST) {
			// need Content-Type & boundary
			GetHttpHeaderValue(p, "Content-Type", eob, buffer, "boundary=", conn->boundary, sizeof(conn->boundary));
			if (*buffer) {
				conn->contentType = CONTENT_OTHER;
				if (os_strncmp(buffer, "application/x-www-form-urlencoded", 33)==0) conn->contentType = CONTENT_FORM;
				if (os_strncmp(buffer, "multipart/form-data", 19)==0) conn->contentType = CONTENT_MULTIPART;
				conn->boundaryLength = os_strlen(conn->boundary);
			} else {
				conn->contentType = CONTENT_NONE;
			}
			// need Content-Length
			GetHttpHeaderValue(p, "Content-Length", eob, buffer, NULL, NULL, 0);
			conn->contentLength = atoi(buffer);
			// skip to body
			while (1) {
				if (*p==0 || p==eob) { httpd_error(conn,413); return; } // Request Entity Too Large
				if (*p=='\r' && *(p+1)=='\n' && *(p+2)=='\r' && *(p+3)=='\n') break;
				p++;
			}
			p+=4;
		} // if (POST)
		char parambuf[1024];
		CopyToTerm(parambuf, p, ' ', '\n', 1024);
		parambuf[1023] = 0;

		char* connType[] = {"None", "multipart/form-data", "form-urlencoded", "other"};
		os_printf("%s %s %s (%u.%u.%u.%u:%u) Content-Type: %s, Content-Length: %d\n",
			conn->method==REQUEST_GET?"GET":"POST",
			conn->path,
			parambuf,
			conn->remote_ip4&0xFF,
			(conn->remote_ip4>>8)&0xFF,
			(conn->remote_ip4>>16)&0xFF,
			(conn->remote_ip4>>24)&0xFF,
			conn->remote_port,
			connType[conn->contentType],
			conn->contentLength
		);
	}

	// GET & POST (form-urlencoded)
	if (conn->contentType==CONTENT_FORM) {
		switch( ParseHttpParams(conn, p, eob) ) {
			case 0: break;                          // processed, not finished
			case 1: ProcessRequest(conn); break;    // process request : pass requested file and parameters to client
			case 2: os_strcpy(conn->boundary,"not enough memory"); httpd_error_ext(conn,418,SRV_EXT_MEMORY,NULL); break;		// I'm a teapot (not enough memory)
		}
	}

	uint16 i;

	// POST (multipart/form-data)
	if (conn->contentType==CONTENT_MULTIPART) {
		i = ParseMultipartData(conn, p, eob); 			 // 0 = not finished, 1 = finished, >1 = error
		if (i) MultipartDataError(conn, i);
	}

	// simple file request
	if (conn->contentType==CONTENT_NONE) {
		ProcessRequest(conn);
	}
}

// ============================================================================
// Check if buffer contains SSI include tag
// EXTREMELY DIRTY because assumes tag is located completely inside one chunk
//
//	In:  buf   : file data chunk
//       clen  : buffer length
//
//	Out: fname : asciiz file name to include
//       end   : pointer to the character just after the tag end
//
//       returns -1 if no tag found, otherwise  
//
sint32 FLASH_CODE HasInclude (char* buf, uint32 clen, char* fname, char** end)
{
	const char* cSSInclude = "<!--#include file=\"";
	//
	sint16 pos = strstrn(buf, cSSInclude, clen);
	if (pos>=0) {
		char* p = buf + pos;
		char* d = fname;
		while (*p++!='"') ;
		while (*p!='"') *d++ = *p++;
		*d = 0;
		while (os_strncmp(p,"-->",3) && p-buf < clen) p++;
		p += 3;
		*end = p;
	}
	return pos;
}

// ============================================================================
// (1) Calculate content length for file with potential SSI's in it, recursively
// (2) Read file with potential SSI's in it, recursively
//
//	In:  f      : source file
//       buf    : target buffer, may be 0 for (1)
//       bufsz  : buffer size, may be 0 for (1)
//       ff     : next file (return value), may by 0 for (1)
//
//	Out: (1)    : total content length for file with potential SSI's
//       (2)    : number of bytes read + ff>0 if opened included file
//
uint32 FLASH_CODE ReadInclude(t_File* f, char* buf, uint32 bufsz, t_File** ff)
{
	uint32 sz = f->size, incsz, puresz = sz;
	uint32 clen;
	bool mallocd = false;
	t_File* finc = 0;

	if (ff && f->pos == f->size) return 0; // completed

	if (os_strcmp(KFS_FileExtension(f->name),"html")==0) {
		// potential SSI's
		if (!buf) {
			// no buffer provided; allocate
			bufsz = system_get_free_heap_size() / 2;
			buf = k_malloc(bufsz, "buf", "ContentLength");
			mallocd = true;
		}
		do {
			clen = _MIN( bufsz, f->size - f->pos );
			KFS_Read(f, buf, clen);

			char fname[128];
			char* inc_end;
			sint16 pos = HasInclude(buf, clen, fname, &inc_end);
			if (pos >= 0) {
				// SSI found
				sint32 offset = inc_end - buf - clen;
				// open next file
				finc = KFS_Open(fname,"r","",0);
				if (finc && !ff) {
					// calculating size: we need to go deeper
					sint32 cut = inc_end - buf - pos;
					sz -= cut;
					puresz -= cut;
					os_printf("including file \"%s\" into \"%s\" at position %d (0x%X)\n", fname, f->name, f->pos+offset, f->pos+offset);
					incsz = ReadInclude(finc, buf, bufsz, 0);
					sz += incsz;
					KFS_Close(finc);
				}
				uint32 newfp = KFS_Seek(f, offset, SEEK_CUR);
				os_printf("\"%s\" moved to %d (f->pos = %d)\n", f->name, newfp, f->pos);
				if (ff) {
					// read mode (2): return inner file and actual data to send
					*ff = finc;
					return pos; // data length
				}
			} else {
				// SSI not found
				if (ff) {
					*ff = 0;
					return clen;
				}
			}
			if (f->pos == f->size) break;
		} while(1);
		if (mallocd) k_free(buf);
	} else {
		// plain file
		if (ff) {
			// read mode (2): just read the file; no need to go deeper
			clen = _MIN( bufsz, f->size - f->pos );
			KFS_Read(f, buf, clen);
			*ff = 0;
			return clen; // data length
		} else {
			// calc size mode (1): return the size of the file
			sz = f->size;
		}
	}
os_printf("size of \"%s\" without includes = %d, with includes = %d\n", f->name, puresz, sz);
	return sz;
}

// ============================================================================
// Load and send file over HTTP
//
//	In:  t_Connection* conn  : connection; conn->path = file to serve
//	Out: none
//
void FLASH_CODE ServeFile (t_HttpOut* httpout)
{
	t_File* f;
	t_Connection* conn = (t_Connection*)httpout;
	sint32 clen = 0;

os_printf("ServeFile(%s)\n",conn->path+1);

	if (os_strcmp(conn->path,"/files")==0) {
		// special case: embedded file manager
		conn->output = files_m_html;
		conn->contentLength = files_m_html_len;
		clen = files_m_html_len;
	} else {
		if (!conn->nFileHandle) {
			f = KFS_Open(conn->path+1,"r","",0);
			if (!f) {
				os_printf("FILE ERROR: %s (%s)\n", KFS_ErrorMessage(), conn->path);
				httpd_error(conn, 404);
				return;
			}
			conn->fileHandle[conn->nFileHandle++] = (uint32)f;
			conn->contentLength = ReadInclude(f,0,0,0); // deep content calculation considering SSI
			conn->outputLength = _MIN( conn->contentLength, system_get_free_heap_size() / 4 );
			conn->output = k_malloc(conn->outputLength, "conn->output", "ServeFile");
			os_strcpy( conn->boundary, f->mime ); // content-type
			dbg_printf(2,"sending file \"%s\" with mime type \"%s\" and content-length %d\n", conn->path+1, f->mime, conn->contentLength);
			KFS_Seek(f, 0, SEEK_SET);
			HttpHeaderSend( httpout, f->mime, NULL, ServeFile ); // headers + callback
			return;
		}

		f = (t_File*)conn->fileHandle[conn->nFileHandle-1];
		t_File* ff;

		clen = ReadInclude(f, conn->output, conn->outputLength, &ff);
		if (f->pos == f->size) {
			KFS_Close(f);
			conn->fileHandle[--conn->nFileHandle] = 0;
		}
		if (ff) {
			conn->fileHandle[conn->nFileHandle++] = (uint32)ff;
		}

		conn->outputData = clen;

		if (!conn->nFileHandle) {
			conn->completed = SEND_LAST_CHUNK;
		}
	}

	HttpSend(httpout);
}

// ============================================================================
// Delete specified file then re-read directory
//
void	FLASH_CODE FileDelete (t_Connection* conn, char* fname)
{
	if (!conn->output)
		if (!KFS_Delete(fname)) os_printf("%s\n",KFS_ErrorMessage());
	FilesRead((t_HttpOut*)conn);
}

// ============================================================================
// Rename specified file
//
void	FLASH_CODE FileRename (t_Connection* conn, char* oldname)
{
	uint8 result;
	if (!conn->output) {
		char* newname = GetParameterValue(conn->param, conn->paramCount, "into");
		if (!KFS_Rename(oldname, newname)) os_printf("%s\n",KFS_ErrorMessage());
		result = KFS_Error();
		conn->output = k_malloc( 128, "conn->output", "FilesRename" );
		os_strcpy(conn->boundary,"application/json");
		if (result==0) {
			os_sprintf(conn->output,"{\"result\":\"ok\"}");
		} else {
			os_sprintf(conn->output,"{\"result\":\"%s\"}", KFS_ErrorMessage());
		}
		conn->outputLength = os_strlen(conn->output);
		conn->contentLength = conn->outputLength;
		HttpSend((t_HttpOut*)conn);
	}
}

// ============================================================================
// Send file system directory listing in JSON
//
void	FLASH_CODE FilesRead (t_HttpOut* httpout)
{
	t_Connection* conn = (t_Connection*)httpout;

	if (!conn->output) {
		// dig content-length
		t_Stats* stats = KFS_Stats();
		t_Dir* dir = KFS_OpenDir (".");
		if (!dir || !stats) {
			httpd_error(conn, 500);
			return;
		}
		conn->outputLength = 128 + dir->len * 90;
		conn->output = k_malloc( conn->outputLength, "conn->output", "FilesRead" ); // TODO: fix size!
		int clen = os_sprintf(conn->output, "{\"total\":%u,\"used\":%u,\"data\":%u,\"free\":%u,\"files\":[", stats->total, stats->used, stats->data, stats->total-stats->used);
		while (dir->Next()) {
			clen += os_sprintf(conn->output+clen, "%s{\"name\":\"%s\",\"size\":%u,\"dt\":%u,\"mime\":\"\"}", dir->pos==1?"":",", dir->file.name, dir->file.size, dir->file.datetime);
		}
		KFS_CloseDir();
		clen += os_sprintf(conn->output+clen, "]}");
		conn->contentLength = clen;
		//
		HttpHeaderSend(httpout, NULL, NULL, FilesRead);
	} else {
		// send json
		HttpSend(httpout);
	}
}

// ============================================================================
// Format file system then re-read directory
//
void	FLASH_CODE SysFormat (t_Connection* conn)
{
	os_printf("SysFormat\n");
	t_Stats* stats = KFS_Stats();
	KFS_Format(stats->start_segment, stats->length_segments);
	FilesRead((t_HttpOut*)conn);
}

// ============================================================================
// Send binary flash dump
//
void	FLASH_CODE FlashDump (t_HttpOut* httpout)
{
	t_File* f;
	t_Connection* conn = (t_Connection*)httpout;

	char headers[64];
	uint32 dump_from = atoi( GetParameterValue(conn->param, conn->paramCount, "fr") );
	uint32 dump_to	 = atoi( GetParameterValue(conn->param, conn->paramCount, "to") );

	if (!conn->output) {
		// contentSent === 0 at this point
		conn->contentLength = (dump_to - dump_from + 1) * SPI_FLASH_SEC_SIZE; // calculate dump size;
		conn->outputLength = system_get_free_heap_size() / 16;
		conn->outputLength *= 4; // round down to 4 bytes
		conn->output = k_malloc(conn->outputLength, "conn->output", "FlashDump");
		os_sprintf(headers,"Content-Disposition: inline; filename=\"dump%02X-%02X.bin", dump_from, dump_to);
		HttpHeaderSend(httpout, "application/octet-stream", headers, FlashDump); // headers
	} else {
		uint32 clen = _MIN( conn->outputLength, conn->contentLength - conn->contentSent );
		if (spi_flash_read(dump_from * 4096 + conn->contentSent, (uint32*)conn->output, clen) != SPI_FLASH_RESULT_OK) {
			os_memset(conn->output, 0xEE, clen);
			os_printf("Error reading flash!\n");
		}
		conn->outputData = clen;
		HttpSend(httpout);
	}
}

// ============================================================================
// Process HTTP request
//
void FLASH_CODE ProcessRequest (t_Connection* conn)
{
	char* val;
	uint8 proc = 0;
	t_HttpOut* httpout = (t_HttpOut*)conn;

	if (os_strcmp(conn->path,"/")==0) os_strcpy(conn->path,"/index.html");

	if (os_strcmp(conn->path,"/files")==0) {
		// special case: embedded file "manager"
		if (val = GetParameterValue(conn->param, conn->paramCount, "ls")) FilesRead(httpout);
		else if (val = GetParameterValue(conn->param, conn->paramCount, "rm")) FileDelete(conn, val);
		else if (val = GetParameterValue(conn->param, conn->paramCount, "rename")) FileRename(conn, val);
		else if (val = GetParameterValue(conn->param, conn->paramCount, "fmt")) SysFormat(conn);
		else if (val = GetParameterValue(conn->param, conn->paramCount, "dump")) FlashDump(httpout);
		else ServeFile(httpout);
	} else {
		//
		if (conn->contentType==CONTENT_NONE && KFS_FileExists(conn->path)) ServeFile(httpout);
		else {
			os_printf("fnProcess()\n");
			if (!fn_Process( conn->method, conn->path, conn->param, conn->paramCount, httpout ))
				if (KFS_FileExists(conn->path))
					ServeFile(httpout);
		}
	}
}

// ============================================================================
// AUX: find connection slot
//
t_Connection* FLASH_CODE httpd_find_conn (struct espconn *pespconn)
{
	uint8 i;
	for (i=0; i<MAX_CONN; i++) {
		if (connData[i].remote_ip4 == *(int32*)pespconn->proto.tcp->remote_ip && connData[i].remote_port == pespconn->proto.tcp->remote_port) {
			connData[i].pespconn = pespconn;
			return &connData[i];
		}
	}
	return NULL;
}

// ============================================================================
// AUX: disconnect
//
#define httpd_TaskQueueLength (MAX_CONN*2)
static os_event_t httpd_TaskQueue[ httpd_TaskQueueLength ];
static volatile os_timer_t disconnectTimer[MAX_CONN*2];
static volatile nDisTimer = 0;
/*
void FLASH_CODE task_httpd_disconnect (os_event_t *events)
{
	char* st[] = {"ESPCONN_NONE","ESPCONN_WAIT","ESPCONN_LISTEN","ESPCONN_CONNECT","ESPCONN_WRITE","ESPCONN_READ","ESPCONN_CLOSE"};
	struct espconn *pespconn = (struct espconn*)events->par;
	sint8 err = espconn_disconnect(pespconn);
	if (err) {
		dbg_printf(3,"	illegal argument: 0x%X\n", pespconn);
		os_printf("  espconn->type = %s\n", pespconn->type==ESPCONN_INVALID ? "INVALID" : (pespconn->type==ESPCONN_TCP ? "TCP" : (pespconn->type==ESPCONN_UDP ? "UDP" : "???")));
		os_printf("  espconn->state = %s\n", st[pespconn->state]);
	}
}
*/
void FLASH_CODE disconnectTimer_cb (void* arg)
{
	char* st[] = {"ESPCONN_NONE","ESPCONN_WAIT","ESPCONN_LISTEN","ESPCONN_CONNECT","ESPCONN_WRITE","ESPCONN_READ","ESPCONN_CLOSE"};
	struct espconn *pespconn = (struct espconn*)arg;
	os_printf("disconnecting %08X\n", pespconn);
	sint8 err = espconn_disconnect(pespconn);
	if (err) {
		dbg_printf(3,"	illegal argument: 0x%X\n", pespconn);
		os_printf("  espconn->type = %s\n", pespconn->type==ESPCONN_INVALID ? "INVALID" : (pespconn->type==ESPCONN_TCP ? "TCP" : (pespconn->type==ESPCONN_UDP ? "UDP" : "???")));
	}
}

// ============================================================================
// HTTP DISCONNECT (espconn)
//
void FLASH_CODE httpd_disconnect (struct espconn *pespconn)
{
	char* st[] = {"ESPCONN_NONE","ESPCONN_WAIT","ESPCONN_LISTEN","ESPCONN_CONNECT","ESPCONN_WRITE","ESPCONN_READ","ESPCONN_CLOSE"};
	os_printf("disconnecting %08X\n", pespconn);
	sint8 err = espconn_disconnect(pespconn);
	if (err) {
		dbg_printf(3,"	illegal argument: 0x%X\n", pespconn);
		os_printf("  espconn->type = %s\n", pespconn->type==ESPCONN_INVALID ? "INVALID" : (pespconn->type==ESPCONN_TCP ? "TCP" : (pespconn->type==ESPCONN_UDP ? "UDP" : "???")));
	}
	return;
	os_printf("manual disconnect\n");
	os_timer_disarm(&disconnectTimer[nDisTimer]);
	os_timer_setfn(&disconnectTimer[nDisTimer], (os_timer_func_t*)disconnectTimer_cb, pespconn);
	os_timer_arm(&disconnectTimer[nDisTimer], 100, 0);
	nDisTimer = (nDisTimer+1) % (MAX_CONN*2);
}

// ============================================================================
// HTTP DISCONNECTED
//
void FLASH_CODE httpd_disconnected (void *arg)
{
	struct espconn *pespconn = arg;
	os_printf("httpd_disconnected (%X)\n", arg);

	t_Connection* conn = httpd_find_conn(pespconn);
	if (conn) {
		conn->remote_ip4 = 0; // remove from active

		uint8 i, n = 0;
		for (i=0; i<MAX_CONN; i++) if (connData[i].remote_ip4) n++;
		os_printf("(-%u) Connections: %u/%u, conn->outputLength=%u, conn->output=%08X\n", conn->remote_port, n, MAX_CONN, conn->outputLength, conn->output);

		if (conn->param) k_free(conn->param);
		if (conn->preload) k_free(conn->preload);
		if (conn->headers) k_free(conn->headers);
		if (conn->outputLength && conn->output) { k_free(conn->output); conn->output = NULL; }
		if (conn->fileHandle[0]) Upload_CloseFile(conn->fileHandle[0]);
	} else {
		os_printf("ERROR: unknown arg in httpd_disconnected()\n");
	}
}

// ============================================================================
// HTTP RECEIVED DATA
//
void FLASH_CODE httpd_received (void *arg, char *pdata, unsigned short length)
{
	struct espconn *pespconn = arg;
	t_Connection* conn = httpd_find_conn(pespconn);
	if (!conn) {
		dbg_printf(2, "Unknown connection!\n");
		// TODO: return 500 Internal Error
		return;
	}
//	os_printf("(=%u) %s\n", conn->remote_port, pdata);
	if (conn->completed) { os_printf("	skipping incoming data for completed connection\n"); return; }
	// parse request, call external callback fns if needed
	ParseHttpRequest(conn, pdata, length);
}

// ============================================================================
// HTTP RECONNECTED
//
void FLASH_CODE httpd_reconnected (void *arg, sint8 err)
{
	struct espconn *pesp_conn = arg;
}

// ============================================================================
// HTTP DATA SENT
//
void FLASH_CODE httpd_sent (void *arg)
{
	struct espconn *pespconn = arg;

	t_Connection* conn = httpd_find_conn(pespconn);

	if (conn->headers) { k_free(conn->headers); conn->headers = 0; }

	if (conn->completed==SEND_COMPLETED) { 
		httpd_disconnect(pespconn); 
	}	else {
		if (conn->dataProvider) {
			conn->dataProvider((t_HttpOut*)conn);
		} else {
			httpd_disconnect(pespconn);
		}
	}
}

// ============================================================================
// HTTP CONNECTED
//
void FLASH_CODE httpd_connected (void *arg)
{
	struct espconn *pespconn = arg;
	t_Connection *cn;
	uint8 i;

	for (i=0; i<MAX_CONN; i++) if (!connData[i].remote_ip4) break;
	if (i==MAX_CONN) {
		os_printf("ERROR: No free slots\n");
		return;
	}

	cn = &connData[i];
	os_memset(cn, 0, sizeof(t_Connection));
	cn->pespconn = pespconn;
	cn->remote_ip4 = *(uint32*)pespconn->proto.tcp->remote_ip;
	cn->remote_port = (uint32)pespconn->proto.tcp->remote_port;

	uint8 n = 0;
	for (i=0; i<MAX_CONN; i++) if (connData[i].remote_ip4) n++;
	os_printf("(+%u, addr=%08X) Connections: %u/%u\n", cn->remote_port, cn->pespconn, n, MAX_CONN);

	espconn_regist_recvcb 	(pespconn, httpd_received );
	espconn_regist_reconcb	(pespconn, httpd_reconnected );
	espconn_regist_disconcb (pespconn, httpd_disconnected );
	espconn_regist_sentcb 	(pespconn, httpd_sent );
}

// ============================================================================
// Init webserver
//
//	In:  port  : 80/443
//
void FLASH_CODE ServerInit (
	uint32 port,
	t_fn_ProcessRequest fn_process
) {
	LOCAL struct espconn esp_conn;
	LOCAL esp_tcp esptcp;
	uint8 i;

	os_printf("Server init\n");
	dbg_printf(3, "Server init()\n");

	for (i=0; i<MAX_CONN; i++) connData[i].pespconn = 0;
	fn_Process = fn_process;

	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = port;
	i = espconn_regist_connectcb(&esp_conn, httpd_connected);

// TODO:
//		espconn_secure_set_default_certificate(default_certificate, default_certificate_len);
//		espconn_secure_set_default_private_key(default_private_key, default_private_key_len);
//		espconn_secure_accept(&esp_conn);

	espconn_accept(&esp_conn);
}

// ============================================================================
// Send HTTP headers
//
//	In:  conn 	 = connection
//			 headers = extra headers, \n delimited, can be NULL
//			 dataProv = user proc to continue sending data. Can be NULL.
//
//			 conn->contentLength	= content-length
//
void	 FLASH_CODE HttpHeaderSend (t_HttpOut *httpout, char* contentType, char *extraHeaders, t_fn_DataProvider dataProv)
{
	t_Connection* conn = (t_Connection*)httpout;
	if (dataProv) conn->dataProvider = dataProv;
	if (contentType) os_strcpy(conn->boundary, contentType);
	httpd_send_header(conn, extraHeaders);
}

// ============================================================================
// Send HTTP body
//
//	In:  conn 		= connection
//
//			 conn->output 			 = output buffer
//			 conn->outputData 	 = current output data length
//			 conn->outputLength  = >0 if output buffer was allocated (and needs to be de-allocated on disconnect)
//			 conn->contentLength = total data to send
//
void	 FLASH_CODE HttpSend (t_HttpOut *httpout)
{
	t_Connection* conn = (t_Connection*)httpout;

	if (conn->completed==SEND_COMPLETED) return;
	if (conn->completed==SEND_LAST_CHUNK) conn->completed = SEND_COMPLETED;

	if (!conn->headerSent) {
		conn->contentLength = os_strlen(conn->output);
		if (!conn->dataProvider) conn->dataProvider = HttpSend;
		httpd_send_header(conn, NULL);
	} else {
		// send contentLength
		if (!conn->contentLength && conn->outputData) conn->contentLength = conn->outputData;
		if (!conn->outputData && conn->contentLength) conn->outputData = conn->contentLength;
		//
		char m = conn->output[conn->outputData-1];
		conn->output[conn->outputData-1] = 0;
//		os_printf("(%u) ====================\n%s\n====================\n", conn->remote_port, conn->output);
		conn->output[conn->outputData-1] = m;
		//
		espconn_send(conn->pespconn, conn->output, conn->outputData);
		conn->contentSent += conn->outputData;
		if (conn->contentSent == conn->contentLength) conn->completed = SEND_COMPLETED;
	}
}

// dataProvider template
/*
void FLASH_CODE dataProvider (t_HttpOut* httpout)
{
	if (!httpout->output) {
		// calc size
		size = ...
		httpout->output = k_malloc(size, "httpout->output", "dataProvider");
	} else {
		// send next portion of data
		os_memcpy(httpout->output, size - httpout->contentSend);
		HttpSend(httpout);
	}
}
*/