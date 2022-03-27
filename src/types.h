#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "common/bip32.h"

/**
 * Enumeration for the status of IO.
 */
typedef enum {
    READY,     /// ready for new event
    RECEIVED,  /// data received
    WAITING    /// waiting
} io_state_e;

/**
 * Enumeration with expected INS of APDU commands.
 */
typedef enum {
    GET_VERSION = 0x03,     /// version of the application
    GET_APP_NAME = 0x04,    /// name of the application
    GET_PUBLIC_KEY = 0x05,  /// public key of corresponding BIP32 path
    SIGN_TX = 0x06          /// sign transaction with BIP32 path
} command_e;

/**
 * Structure with fields of APDU command.
 */
typedef struct {
    uint8_t cla;    /// Instruction class
    command_e ins;  /// Instruction code
    uint8_t p1;     /// Instruction parameter 1
    uint8_t p2;     /// Instruction parameter 2
    uint8_t lc;     /// Lenght of command data
    uint8_t *data;  /// Command data
} command_t;
