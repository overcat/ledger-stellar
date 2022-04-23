#pragma once

#include <stddef.h>  // size_t
#include <stdint.h>  // uint*_t

#include "common/bip32.h"
#include "transaction/transaction_types.h"

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
    INS_GET_PUBLIC_KEY = 0x02,         /// public key of corresponding BIP32 path
    INS_SIGN_TX = 0x04,                /// sign transaction with BIP32 path
    INS_GET_APP_CONFIGURATION = 0x06,  /// app configuration of the application
    INS_SIGN_TX_HASH = 0x08,       /// sign transaction in hash mode
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

/**
 * Enumeration with user request type.
 */
typedef enum {
    CONFIRM_ADDRESS,          /// confirm address derived from public key
    CONFIRM_TRANSACTION,      /// confirm transaction information
    CONFIRM_TRANSACTION_HASH  /// confirm transaction hash information
} request_type_e;

/**
 * Enumeration with parsing state.
 */
typedef enum {
    STATE_NONE,     /// No state
    STATE_PARSED,   /// Transaction data parsed
    STATE_APPROVED  /// Transaction data approved
} state_e;

/**
 * Structure for global context.
 */
typedef struct {
    tx_ctx_t tx_info;
    uint8_t hash[HASH_SIZE];
    uint32_t bip32_path[MAX_BIP32_PATH];  /// BIP32 path
    uint8_t raw_public_key[32];
    uint8_t signature[64];
    state_e state;  /// state of the context
    request_type_e req_type;              /// user request
    uint8_t bip32_path_len;               /// length of BIP32 path
} global_ctx_t;

typedef struct {
    uint64_t amount;
    uint64_t fees;
    char destination[ED25519_PUBLIC_STRKEY_LEN + 1];  // ed25519 address only
    char memo[20];
} swap_values_t;