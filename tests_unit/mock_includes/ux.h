
/*******************************************************************************
 *   Ledger Nano S - Secure firmware
 *   (c) 2021 Ledger
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
 ********************************************************************************/

#pragma once

typedef struct bagl_element_e bagl_element_t;

// callback returns NULL when element must not be redrawn (with a changing color or what so ever)
typedef const bagl_element_t* (*bagl_element_callback_t)(const bagl_element_t* element);

// a graphic element is an element with defined text and actions depending on user touches
struct bagl_element_e {
    const char* text;
};

/**
 * Common structure for applications to perform asynchronous UX aside IO operations
 */
typedef struct ux_state_s ux_state_t;

struct ux_state_s {
    unsigned char stack_count;  // initialized @0 by the bolos ux initialize
    // bolos_task_status_t exit_code;
};