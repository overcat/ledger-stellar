#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "os.h"
#include "ux.h"

bool app_mode_hash_signing_enabled();

void app_mode_change_hash_signing_setting();