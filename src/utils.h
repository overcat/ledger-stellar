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

bool print_binary(const uint8_t *in,
                  size_t in_len,
                  char *out,
                  size_t out_len,
                  uint8_t num_chars_l,
                  uint8_t num_chars_r);

bool print_time(uint64_t seconds, char *out, size_t out_len);

bool print_asset(const Asset *asset, uint8_t network_id, char *out, size_t out_len);

void print_flags(uint32_t flags, char *out, size_t out_len);

void print_trust_line_flags(uint32_t flags, char *out, size_t out_len);

bool print_amount(uint64_t amount,
                  const Asset *asset,
                  uint8_t network_id,
                  char *out,
                  size_t out_len);

bool print_account_id(AccountID account_id,
                      char *out,
                      size_t out_len,
                      uint8_t num_chars_l,
                      uint8_t num_chars_r);

bool print_muxed_account(const MuxedAccount *muxed_account,
                         char *out,
                         size_t out_len,
                         uint8_t num_chars_l,
                         uint8_t num_chars_r);

bool print_summary(const char *in,
                   char *out,
                   size_t out_len,
                   uint8_t num_chars_l,
                   uint8_t num_chars_r);

bool print_uint(uint64_t num, char *out, size_t out_len);

bool print_int(int64_t num, char *out, size_t out_len);

bool base64_encode(const uint8_t *data, size_t in_len, char *out, size_t out_len);