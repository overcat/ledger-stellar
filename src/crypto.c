/*****************************************************************************
 *   Ledger App Boilerplate.
 *   (c) 2020 Ledger SAS.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *****************************************************************************/

#include <stdint.h>   // uint*_t
#include <string.h>   // explicit_bzero
#include <stdbool.h>  // bool

#include "crypto.h"
#include "globals.h"

#define STELLAR_SEED_KEY "ed25519 seed"

int crypto_derive_private_key(cx_ecfp_private_key_t *private_key,
                              const uint32_t *bip32_path,
                              uint8_t bip32_path_len) {
    uint8_t raw_private_key[32] = {0};

    BEGIN_TRY {
        TRY {
            // derive the seed with bip32_path
            os_perso_derive_node_with_seed_key(HDW_ED25519_SLIP10,
                                               CX_CURVE_Ed25519,
                                               bip32_path,
                                               bip32_path_len,
                                               raw_private_key,
                                               NULL,
                                               (unsigned char *) STELLAR_SEED_KEY,
                                               sizeof(STELLAR_SEED_KEY));
            // new private_key from raw
            cx_ecfp_init_private_key(CX_CURVE_Ed25519,
                                     raw_private_key,
                                     sizeof(raw_private_key),
                                     private_key);
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY {
            explicit_bzero(&raw_private_key, sizeof(raw_private_key));
        }
    }
    END_TRY;

    return 0;
}

// converts little endian 32 byte public key to big endian 32 byte public key
void raw_public_key_le_to_be(cx_ecfp_public_key_t *public_key, uint8_t raw_public_key[static 32]) {
    // copy public key little endian to big endian
    for (uint8_t i = 0; i < 32; i++) {
        raw_public_key[i] = public_key->W[64 - i];
    }
    // set sign bit
    if ((public_key->W[32] & 1) != 0) {
        raw_public_key[31] |= 0x80;
    }
}

int crypto_init_public_key(cx_ecfp_private_key_t *private_key,
                           cx_ecfp_public_key_t *public_key,
                           uint8_t raw_public_key[static 32]) {
    BEGIN_TRY {
        TRY {
            // generate corresponding public key
            cx_ecfp_generate_pair(CX_CURVE_Ed25519, public_key, private_key, 1);
        }
        CATCH_OTHER(e) {
            THROW(e);
        }
        FINALLY {
            explicit_bzero(private_key, sizeof(cx_ecfp_private_key_t));
        }
    }
    END_TRY;

    raw_public_key_le_to_be(public_key, raw_public_key);

    return 0;
}
