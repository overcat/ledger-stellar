#include "types.h"
/**  parse a bip32 path from a byte stream */

/**  base32 encode public key */
void encode_public_key(const uint8_t *in, char *out);

/** base32 encode pre-auth transaction hash */
void encode_pre_auth_key(const uint8_t *in, char *out);

/** base32 encode sha256 hash */
void encode_hash_x_key(const uint8_t *in, char *out);

/** raw public key to base32 encoded (summarized) address */
void print_public_key(const uint8_t *in, char *out, uint8_t numCharsL, uint8_t numCharsR);

/**  base32 encode muxed account */
void encode_muxed_account_(const MuxedAccount *in, char *out);

/** raw muxed account to base32 encoded muxed address */
void print_muxed_account(const MuxedAccount *in, char *out, uint8_t numCharsL, uint8_t numCharsR);

/** output first numCharsL of input + last numCharsR of input separated by ".." */
void print_summary_(const char *in, char *out, uint8_t numCharsL, uint8_t numCharsR);

/** raw byte buffer to hexadecimal string representation.
 * len is length of input, provided output must be twice that size */
void print_binary_(const uint8_t *in, char *out, uint8_t len);

/** raw byte buffer to summarized hexadecimal string representation
 * len is length of input, provided output must be at least length 19 */
void print_binary__summary(const uint8_t *in, char *out, uint8_t len);

/** raw amount integer to asset-qualified string representation */
int print_amount(uint64_t amount,
                 const Asset *asset,
                 uint8_t network_id,
                 char *out,
                 size_t out_len);

/** concatenate assetCode and assetIssuer summary */
void print_asset_t(const Asset *asset, uint8_t network_id, char *out, size_t out_len);

/** asset name */
int print_asset_name(const Asset *asset, uint8_t network_id, char *out, size_t out_len);

/** concatenate code and issuer */
void print_asset(const char *code, char *issuer, bool isNative, char *out, size_t out_len);

/** "XLM" or "native" depending on the network id */
void print_native_asset_code(uint8_t network, char *out, size_t out_len);

/** string representation of flags present */
void print_flags(uint32_t flags, char *out, size_t out_len);

/** string representation of trust line flags present */
void print_trust_line_flags(uint32_t flags, char *out, size_t out_len);

/** integer to string for display of sequence number */
int print_int_(int64_t num, char *out, size_t out_len);

/** integer to string for display of offerid, sequence number, threshold weights, etc */
int print_uint_(uint64_t num, char *out, size_t out_len);

/** base64 encoding function used to display managed data values */
void base64_encode(const uint8_t *data, int inLen, char *out);

/** hex representation of flags claimable balance id */
void print_claimable_balance_id_(const ClaimableBalanceID *claimableBalanceID, char *out);

/** converts the timestamp in seconds to a readable utc time string */
bool print_time_(uint64_t timestamp_in_seconds, char *out, size_t out_len);