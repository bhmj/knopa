#ifndef _KNOPA_SERVER_H
#define _KNOPA_SERVER_H

#include "kmem.h"
#include <c_types.h>
#include <ip_addr.h>
#include <espconn.h>

typedef enum {
  REQUEST_UNKNOWN = 0,
  REQUEST_GET,
  REQUEST_POST
} enRequestMethod;

typedef struct {
  char* output;                // output buffer
  uint32 outputLength;         // length of allocated buffer
  uint32 outputData;           // length of data to send
  uint32 contentLength;        // 
  uint32 contentSent;          // works together with contentLength
} t_HttpOut;

typedef void (*t_fn_DataProvider)  (t_HttpOut*); // data provider callback

// ============================================================================

//typedef uint32 (* OpenFile_t)  (char* fname, char* mime);
//typedef uint32 (* WriteFile_t) (uint32 handle, char* buf, uint32 length);
//typedef uint32 (* CloseFile_t) (uint32 handle);

typedef bool (* t_fn_ProcessRequest) (enRequestMethod method, char* path, char* params, int paramCount, t_HttpOut* httpout);

void   FLASH_CODE ServerInit (uint32 port, t_fn_ProcessRequest fn_process);

char*  FLASH_CODE GetParameterValue (char* params, int paramCount, char* param);

void   FLASH_CODE HttpHeaderSend (t_HttpOut *httpout, char* contentType, char *headers, t_fn_DataProvider dataProv);

void   FLASH_CODE HttpSend (t_HttpOut *httpout);

void   FLASH_CODE HttpError (t_HttpOut *httpout, uint16 httpCode);

#endif
