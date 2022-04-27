#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "os.h"
#include "ux.h"

/**
 * Query whether hash signing model is enabled.
 *
 * @return true if enables, false otherwise.
 */
bool app_mode_hash_signing_enabled();

/**
 * If the hash signing model is enabled, disable it; otherwise, enable it.
 */
void app_mode_change_hash_signing_setting();