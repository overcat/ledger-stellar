#pragma once

/**
 * Handler gor INS_GET_APP_CONFIGURATION command. Send APDU response with version
 * of the application.
 *
 * @see MAJOR_VERSION, MINOR_VERSION and PATCH_VERSION in Makefile.
 * @see
 *
 * @return zero or positive integer if success, negative integer otherwise.
 *
 */
int handler_get_app_configuration(void);
