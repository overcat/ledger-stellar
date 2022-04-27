/*****************************************************************************
 *   Ledger Stellar App.
 *   (c) 2022 Ledger SAS.
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

#include "app_mode.h"

typedef struct {
    uint8_t hash_signing_enabled;
} app_mode_persistent_t;

app_mode_persistent_t const N_appmode_impl __attribute__((aligned(64)));
#define N_appmode (*(volatile app_mode_persistent_t *) PIC(&N_appmode_impl))

bool app_mode_hash_signing_enabled() {
    return N_appmode.hash_signing_enabled;
}

void app_mode_change_hash_signing_setting() {
    app_mode_persistent_t mode;
    mode.hash_signing_enabled = !app_mode_hash_signing_enabled();
    nvm_write((void *) PIC(&N_appmode_impl), (void *) &mode, sizeof(app_mode_persistent_t));
}
