#pragma once

/**
 * Instruction class of the Stellar application.
 */
#define CLA 0xE0

/**
 * Maximum length of MAJOR_VERSION || MINOR_VERSION || PATCH_VERSION.
 */
#define APPVERSION_LEN 3

/**
 * Maximum transaction length (bytes).
 */
#define MAX_TRANSACTION_LEN 510

/**
 * signature length (bytes).
 */
#define SIGNATURE_SIZE 64

/*
 * Captions don't scroll so there is no use in having more capacity than can fit on screen at once.
 */
#define DETAIL_CAPTION_MAX_SIZE 20

/*
 * DETAIL_VALUE_MAX_SIZE value of 89 is due to the maximum length of managed data value which can be
 * 64 bytes long. Managed data values are displayed as base64 encoded strings, which are
 * 4*((len+2)/3) characters long. (An additional slot is required for the end-of-string character of
 * course)
 */
#define DETAIL_VALUE_MAX_SIZE 89

/* Max transaction size */
#define MAX_RAW_TX 1120