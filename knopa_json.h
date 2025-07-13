#ifndef _KNOPA_JSON_H
#define _KNOPA_JSON_H

#include "kmem.h" // FLASH_CODE
#include "knopa_string.h"

// ------------------------------------------------------------------------------------
// Read JSON into {data} using {scheme}
//
// Scheme:
//   
//  object deefinition:
//    {sizeof|field/offset,...|type,...}
//
//  where
//    sizeof : size of target struct
//    field  : field name in JSON
//    offset : offset in target struct
//    type   : b,w,d,s,n for byte, word, dword, string, CR-delimited strings respectively
//             also type can be array definition
//
//    each field must 
//
//  array definition:
//    [maxlen|object]            -- array of objects
//    or
//    [maxlen|sizeof|type]       -- array of values
//
//  where
//    maxlen : maximum array capacity (elements)
//    object : object definition (see above)
//    sizeof : size of a single value
//    type   : field type
//

//
// Examples:
//  
//  [8{500|code/0,name/16,conditions/84,actions/292|s,s,[4{52|cid/0,ival/4|w,[8|0|n]}],[4{52|aid/0,parm/4|w,[8|0|n]}]}]
//    -- array, maxlen=8, each element is an object (struct) of size 500, object fields are: code (offset 0 in struct), name (offset 16 in struct), conditions, actions
//    -- code and name are strings
//    -- conditions is an array, maxlen=4, each element is an object (struct) of size 52, object fields are: cid (offset 0), ival (offset 4)
//      -- cid is int, ival is an array of \n-delimited strings stored sequentially one after another
//
//  [32{174|hwid/0,type/2,protocol/4,params/20|w,w,s,[4|0|n]}]
//    -- array, maxlen=32, each element is an object (struct) of size 174, object fields are: hwid (offset 0), type (offset 2), protocol (offset 4), params (offset 20)
//    -- hwid and type are words, protocol is a string, params is an array, maxlen=4, of \n-delimited strings stored sequentially one after another
//

char* FLASH_CODE ParseJson (char* json, void* data, char* scheme);

#endif
