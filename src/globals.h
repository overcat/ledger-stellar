#pragma once

#include <stdint.h>

#include "ux.h"
#include "types.h"

#include "io.h"

/**
 * Global buffer for interactions between SE and MCU.
 */
extern uint8_t G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

/**
 * Global variable with the lenght of APDU response to send back.
 */
extern uint32_t G_output_len;

/**
 * Global structure to perform asynchronous UX aside IO operations.
 */
extern ux_state_t G_ux;

/**
 * Global structure with the parameters to exchange with the BOLOS UX application.
 */
extern bolos_ux_params_t G_ux_params;

/**
 * Global enumeration with the state of IO (READY, RECEIVING, WAITING).
 */
extern io_state_e G_io_state;

/**
 * Global context for user requests.
 */
extern global_ctx_t G_context;

extern swap_values_t G_swap_values;

extern char detailCaption[DETAIL_CAPTION_MAX_SIZE];
extern char detailValue[DETAIL_VALUE_MAX_SIZE];

extern volatile uint8_t current_state;  // Dynamic screen?
#define INSIDE_BORDERS 0
#define OUT_OF_BORDERS 1