#pragma once

#include <stdint.h>   // uint*_t
#include <stddef.h>   // size_t
#include <stdbool.h>  // bool
#include "types.h"

bool encode_ed25519_public_key(const uint8_t raw_public_key[static RAW_ED25519_PUBLIC_KEY_LENGTH],
                               char *out,
                               size_t out_len);