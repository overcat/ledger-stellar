#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include "types.h"

bool encode_ed25519_public_key(const uint8_t raw_public_key[static RAW_ED25519_PUBLIC_KEY_SIZE],
                               char *out,
                               size_t out_len);

bool encode_hash_x_key(const uint8_t raw_hash_x[static RAW_HASH_X_KEY_SIZE],
                       char *out,
                       size_t out_len);

bool encode_pre_auth_x_key(const uint8_t raw_pre_auth_tx[static RAW_PRE_AUTH_TX_KEY_SIZE],
                           char *out,
                           size_t out_len);

bool encode_muxed_account(const MuxedAccount *raw_muxed_account, char *out, size_t out_len);

bool print_claimable_balance_id(const ClaimableBalanceID *claimable_balance_id,
                                char *out,
                                size_t out_len);

bool print_binary(const uint8_t *in, size_t in_len, char *out, size_t out_len);

bool print_time(uint64_t seconds, char *out, size_t out_len);
