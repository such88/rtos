#ifndef _STATE_ERROR_CODE_H
#define _STATE_ERROR_CODE_H

typedef enum {
    STATE_OK = 0,          // No error
    STATE_ERROR = -1,      // General error
    STATE_TIMEOUT = -2,    // Operation timed out
    STATE_INVALID_PARAM = -3, // Invalid parameter passed
    STATE_NOT_FOUND = -4,  // Item not found
    STATE_BUSY = -5,       // Resource is busy
    STATE_UNSUPPORTED = -6, // Operation not supported
} StateErrorCode;

typedef enum {
    OFF = 0,
    ON = 1
} StateBoolean;

#endif