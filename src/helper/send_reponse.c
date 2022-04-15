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

#include <stdint.h>  // uint*_t
#include <string.h>  // memmove

#include "send_response.h"
#include "../globals.h"
#include "../sw.h"
#include "common/buffer.h"

int helper_send_response_pubkey() {
    uint8_t resp[RAW_ED25519_PUBLIC_KEY_LENGTH] = {0};
    memmove(resp, G_context.raw_public_key, RAW_ED25519_PUBLIC_KEY_LENGTH);
    size_t offset = RAW_ED25519_PUBLIC_KEY_LENGTH;
    return io_send_response(&(const buffer_t){.ptr = resp, .size = offset, .offset = 0}, SW_OK);
}

int helper_send_response_sig() {
    return io_send_response(&(const buffer_t){.ptr = G_context.signature, .size = 64, .offset = 0},
                            SW_OK);
}
