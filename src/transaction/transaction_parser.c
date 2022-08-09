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

#include <stdint.h>
#include <string.h>
// #include "os.h"
#include "../types.h"
#include "../sw.h"
#include "common/buffer.h"
#include "transaction_parser.h"

#define READER_CHECK(x)         \
    {                           \
        if (!(x)) return false; \
    }

/* SHA256("Public Global Stellar Network ; September 2015") */
static const uint8_t NETWORK_ID_PUBLIC_HASH[32] = {
    0x7a, 0xc3, 0x39, 0x97, 0x54, 0x4e, 0x31, 0x75, 0xd2, 0x66, 0xbd, 0x02, 0x24, 0x39, 0xb2, 0x2c,
    0xdb, 0x16, 0x50, 0x8c, 0x01, 0x16, 0x3f, 0x26, 0xe5, 0xcb, 0x2a, 0x3e, 0x10, 0x45, 0xa9, 0x79};

/* SHA256("Test SDF Network ; September 2015") */
static const uint8_t NETWORK_ID_TEST_HASH[32] = {
    0xce, 0xe0, 0x30, 0x2d, 0x59, 0x84, 0x4d, 0x32, 0xbd, 0xca, 0x91, 0x5c, 0x82, 0x03, 0xdd, 0x44,
    0xb3, 0x3f, 0xbb, 0x7e, 0xdc, 0x19, 0x05, 0x1e, 0xa3, 0x7a, 0xbe, 0xdf, 0x28, 0xec, 0xd4, 0x72};

static void buffer_advance(buffer_t *buffer, size_t num_bytes) {
    buffer_seek_cur(buffer, num_bytes);
}

static bool buffer_read32(buffer_t *buffer, uint32_t *n) {
    return buffer_read_u32(buffer, n, BE);
}

static bool buffer_read64(buffer_t *buffer, uint64_t *n) {
    return buffer_read_u64(buffer, n, BE);
}

static bool buffer_read_bool(buffer_t *buffer, bool *b) {
    uint32_t val;

    if (!buffer_read32(buffer, &val)) {
        return false;
    }
    if (val != 0 && val != 1) {
        return false;
    }
    *b = val == 1 ? true : false;
    return true;
}

static bool buffer_read_bytes(buffer_t *buffer, uint8_t *out, size_t size) {
    if (buffer->size - buffer->offset < size) {
        return false;
    }
    memcpy(out, buffer->ptr + buffer->offset, size);
    buffer->offset += size;
    return true;
}

static size_t num_bytes(size_t size) {
    size_t remainder = size % 4;
    if (remainder == 0) {
        return size;
    }
    return size + 4 - remainder;
}

static bool check_padding(const uint8_t *buffer, size_t offset, size_t length) {
    unsigned int i;
    for (i = 0; i < length - offset; i++) {
        if (buffer[offset + i] != 0x00) {
            return false;
        }
    }
    return true;
}

bool read_binary_string_ptr(buffer_t *buffer,
                            const uint8_t **string,
                            size_t *out_len,
                            size_t max_length) {
    /* max_length does not include terminal null character */
    uint32_t size;

    if (!buffer_read32(buffer, &size)) {
        return false;
    }
    if (size > max_length || !buffer_can_read(buffer, num_bytes(size))) {
        return false;
    }
    if (!check_padding(buffer->ptr + buffer->offset, size,
                       num_bytes(size))) {  // security check
        return false;
    }
    *string = (uint8_t *) buffer->ptr + buffer->offset;
    if (out_len) {
        *out_len = size;
    }
    buffer_advance(buffer, num_bytes(size));
    return true;
}

typedef bool (*xdr_type_reader)(buffer_t *, void *);

bool read_optional_type(buffer_t *buffer, xdr_type_reader reader, void *dst, bool *opted) {
    bool is_present;

    if (!buffer_read_bool(buffer, &is_present)) {
        return false;
    }
    if (is_present) {
        if (opted) {
            *opted = true;
        }
        return reader(buffer, dst);
    } else {
        if (opted) {
            *opted = false;
        }
        return true;
    }
}

bool read_signer_key(buffer_t *buffer, signer_key_t *key) {
    uint32_t signer_type;

    READER_CHECK(buffer_read32(buffer, &signer_type))
    key->type = signer_type;

    switch (signer_type) {
        case SIGNER_KEY_TYPE_ED25519:
            READER_CHECK(buffer_can_read(buffer, 32))
            key->ed25519 = buffer->ptr + buffer->offset;
            buffer_advance(buffer, 32);
            return true;
        case SIGNER_KEY_TYPE_PRE_AUTH_TX:
            READER_CHECK(buffer_can_read(buffer, 32))
            key->pre_auth_tx = buffer->ptr + buffer->offset;
            buffer_advance(buffer, 32);
            return true;
        case SIGNER_KEY_TYPE_HASH_X:
            READER_CHECK(buffer_can_read(buffer, 32))
            key->hash_x = buffer->ptr + buffer->offset;
            buffer_advance(buffer, 32);
            return true;
        case SIGNER_KEY_TYPE_ED25519_SIGNED_PAYLOAD:
            READER_CHECK(buffer_can_read(buffer, 32))
            key->ed25519_signed_payload.ed25519 = buffer->ptr + buffer->offset;
            buffer_advance(buffer, 32);
            uint32_t payload_length;
            READER_CHECK(buffer_read32(buffer, &payload_length))
            // valid length [1, 64]
            if (payload_length == 0 || payload_length > 64) {
                return false;
            }
            key->ed25519_signed_payload.payload_len = payload_length;
            payload_length += (4 - payload_length % 4) % 4;
            READER_CHECK(buffer_can_read(buffer, payload_length))
            key->ed25519_signed_payload.payload = buffer->ptr + buffer->offset;
            buffer_advance(buffer, payload_length);
            return true;
        default:
            return false;
    }
}

bool read_account_id(buffer_t *buffer, const uint8_t **account_id) {
    uint32_t account_type;

    READER_CHECK(buffer_read32(buffer, &account_type) || account_type != PUBLIC_KEY_TYPE_ED25519)
    READER_CHECK(buffer_can_read(buffer, 32))
    *account_id = buffer->ptr + buffer->offset;
    buffer_advance(buffer, 32);
    return true;
}

bool read_muxed_account(buffer_t *buffer, muxed_account_t *muxed_account) {
    uint32_t crypto_key_type;
    READER_CHECK(buffer_read32(buffer, &crypto_key_type))
    muxed_account->type = crypto_key_type;

    switch (muxed_account->type) {
        case KEY_TYPE_ED25519:
            READER_CHECK(buffer_can_read(buffer, 32))
            muxed_account->ed25519 = buffer->ptr + buffer->offset;
            buffer_advance(buffer, 32);
            return true;
        case KEY_TYPE_MUXED_ED25519:
            READER_CHECK(buffer_read64(buffer, &muxed_account->med25519.id))
            READER_CHECK(buffer_can_read(buffer, 32))
            muxed_account->med25519.ed25519 = buffer->ptr + buffer->offset;
            buffer_advance(buffer, 32);
            return true;
        default:
            return false;
    }
}

bool read_time_bounds(buffer_t *buffer, time_bounds_t *bounds) {
    READER_CHECK(buffer_read64(buffer, &bounds->min_time))
    return buffer_read64(buffer, &bounds->max_time);
}

bool read_ledger_bounds(buffer_t *buffer, ledger_bounds_t *ledger_bounds) {
    READER_CHECK(buffer_read32(buffer, (uint32_t *) &ledger_bounds->min_ledger))
    READER_CHECK(buffer_read32(buffer, (uint32_t *) &ledger_bounds->max_ledger))
    return true;
}

bool read_extra_signers(buffer_t *buffer) {
    uint32_t length;
    READER_CHECK(buffer_read32(buffer, &length))
    if (length > 2) {  // maximum length is 2
        return false;
    }
    signer_key_t signer_key;
    for (uint32_t i = 0; i < length; i++) {
        READER_CHECK(read_signer_key(buffer, &signer_key))
    }
    return true;
}

bool read_preconditions(buffer_t *buffer, preconditions_t *cond) {
    uint32_t precondition_type;
    READER_CHECK(buffer_read32(buffer, &precondition_type))
    switch (precondition_type) {
        case PRECOND_NONE:
            cond->time_bounds_present = false;
            cond->min_seq_num_present = false;
            cond->ledger_bounds_present = false;
            cond->min_seq_ledger_gap = 0;
            cond->min_seq_age = 0;
            return true;
        case PRECOND_TIME:
            cond->time_bounds_present = true;
            READER_CHECK(read_time_bounds(buffer, &cond->time_bounds))
            cond->min_seq_num_present = false;
            cond->ledger_bounds_present = false;
            cond->min_seq_ledger_gap = 0;
            cond->min_seq_age = 0;
            return true;
        case PRECOND_V2:
            READER_CHECK(read_optional_type(buffer,
                                            (xdr_type_reader) read_time_bounds,
                                            &cond->time_bounds,
                                            &cond->time_bounds_present))
            READER_CHECK(read_optional_type(buffer,
                                            (xdr_type_reader) read_ledger_bounds,
                                            &cond->ledger_bounds,
                                            &cond->ledger_bounds_present))
            READER_CHECK(read_optional_type(buffer,
                                            (xdr_type_reader) buffer_read64,
                                            (uint64_t *) &cond->min_seq_num,
                                            &cond->min_seq_num_present))
            READER_CHECK(buffer_read64(buffer, (uint64_t *) &cond->min_seq_age))
            READER_CHECK(buffer_read32(buffer, &cond->min_seq_ledger_gap))
            READER_CHECK(read_extra_signers(buffer))
            return true;
        default:
            return false;
    }
}

bool read_memo(buffer_t *buffer, memo_t *memo) {
    uint32_t type;

    if (!buffer_read32(buffer, &type)) {
        return 0;
    }
    memo->type = type;
    switch (memo->type) {
        case MEMO_NONE:
            return true;
        case MEMO_ID:
            return buffer_read64(buffer, &memo->id);
        case MEMO_TEXT: {
            size_t size;
            READER_CHECK(
                read_binary_string_ptr(buffer, (const uint8_t **) &memo->text.text, &size, 28))
            memo->text.text_size = size;
            return true;
        }
        case MEMO_HASH:
            if (buffer->size - buffer->offset < HASH_SIZE) {
                return false;
            }
            memo->hash = buffer->ptr + buffer->offset;
            buffer->offset += HASH_SIZE;
            return true;
        case MEMO_RETURN:
            if (buffer->size - buffer->offset < HASH_SIZE) {
                return false;
            }
            memo->return_hash = buffer->ptr + buffer->offset;
            buffer->offset += HASH_SIZE;
            return true;
        default:
            return false;  // unknown memo type
    }
}

bool read_alpha_num4_asset(buffer_t *buffer, alpha_num4_t *asset) {
    READER_CHECK(buffer_can_read(buffer, 4))
    asset->asset_code = (const char *) buffer->ptr + buffer->offset;
    buffer_advance(buffer, 4);
    READER_CHECK(read_account_id(buffer, &asset->issuer))
    return true;
}

bool read_alpha_num12_asset(buffer_t *buffer, alpha_num12_t *asset) {
    READER_CHECK(buffer_can_read(buffer, 12))
    asset->asset_code = (const char *) buffer->ptr + buffer->offset;
    buffer_advance(buffer, 12);
    READER_CHECK(read_account_id(buffer, &asset->issuer))
    return true;
}

bool read_asset(buffer_t *buffer, asset_t *asset) {
    uint32_t asset_type;

    READER_CHECK(buffer_read32(buffer, &asset_type))
    asset->type = asset_type;
    switch (asset->type) {
        case ASSET_TYPE_NATIVE: {
            return true;
        }
        case ASSET_TYPE_CREDIT_ALPHANUM4: {
            return read_alpha_num4_asset(buffer, &asset->alpha_num4);
        }
        case ASSET_TYPE_CREDIT_ALPHANUM12: {
            return read_alpha_num12_asset(buffer, &asset->alpha_num12);
        }
        default:
            return false;  // unknown asset type
    }
}

bool read_trust_line_asset(buffer_t *buffer, trust_line_asset_t *asset) {
    uint32_t asset_type;

    READER_CHECK(buffer_read32(buffer, &asset_type))
    asset->type = asset_type;
    switch (asset->type) {
        case ASSET_TYPE_NATIVE: {
            return true;
        }
        case ASSET_TYPE_CREDIT_ALPHANUM4: {
            return read_alpha_num4_asset(buffer, &asset->alpha_num4);
        }
        case ASSET_TYPE_CREDIT_ALPHANUM12: {
            return read_alpha_num12_asset(buffer, &asset->alpha_num12);
        }
        case ASSET_TYPE_POOL_SHARE: {
            return buffer_read_bytes(buffer, asset->liquidity_pool_id, LIQUIDITY_POOL_ID_SIZE);
        }
        default:
            return false;  // unknown asset type
    }
}

bool read_liquidity_pool_parameters(buffer_t *buffer,
                                    liquidity_pool_parameters_t *liquidity_pool_parameters) {
    uint32_t liquidity_pool_type;
    READER_CHECK(buffer_read32(buffer, &liquidity_pool_type))
    switch (liquidity_pool_type) {
        case LIQUIDITY_POOL_CONSTANT_PRODUCT: {
            READER_CHECK(read_asset(buffer, &liquidity_pool_parameters->constant_product.asset_a))
            READER_CHECK(read_asset(buffer, &liquidity_pool_parameters->constant_product.asset_b))
            READER_CHECK(
                buffer_read32(buffer,
                              (uint32_t *) &liquidity_pool_parameters->constant_product.fee))
            return true;
        }
        default:
            return false;
    }
}

bool read_change_trust_asset(buffer_t *buffer, change_trust_asset_t *asset) {
    uint32_t asset_type;

    READER_CHECK(buffer_read32(buffer, &asset_type))
    asset->type = asset_type;
    switch (asset->type) {
        case ASSET_TYPE_NATIVE: {
            return true;
        }
        case ASSET_TYPE_CREDIT_ALPHANUM4: {
            return read_alpha_num4_asset(buffer, &asset->alpha_num4);
        }
        case ASSET_TYPE_CREDIT_ALPHANUM12: {
            return read_alpha_num12_asset(buffer, &asset->alpha_num12);
        }
        case ASSET_TYPE_POOL_SHARE: {
            return read_liquidity_pool_parameters(buffer, &asset->liquidity_pool);
        }
        default:
            return false;  // unknown asset type
    }
}

bool read_create_account(buffer_t *buffer, create_account_op_t *create_account_op) {
    READER_CHECK(read_account_id(buffer, &create_account_op->destination))
    return buffer_read64(buffer, (uint64_t *) &create_account_op->starting_balance);
}

bool read_payment(buffer_t *buffer, payment_op_t *payment_op) {
    READER_CHECK(read_muxed_account(buffer, &payment_op->destination))

    READER_CHECK(read_asset(buffer, &payment_op->asset))

    return buffer_read64(buffer, (uint64_t *) &payment_op->amount);
}

bool read_path_payment_strict_receive(buffer_t *buffer, path_payment_strict_receive_op_t *op) {
    uint32_t path_len;

    READER_CHECK(read_asset(buffer, &op->send_asset))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->send_max))
    READER_CHECK(read_muxed_account(buffer, &op->destination))
    READER_CHECK(read_asset(buffer, &op->dest_asset))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->dest_amount))

    READER_CHECK(buffer_read32(buffer, (uint32_t *) &path_len))
    op->path_len = path_len;
    if (op->path_len > 5) {
        return false;
    }
    for (int i = 0; i < op->path_len; i++) {
        READER_CHECK(read_asset(buffer, &op->path[i]))
    }
    return true;
}

bool read_allow_trust(buffer_t *buffer, allow_trust_op_t *op) {
    uint32_t asset_type;

    READER_CHECK(read_account_id(buffer, &op->trustor))
    READER_CHECK(buffer_read32(buffer, &asset_type))

    switch (asset_type) {
        case ASSET_TYPE_CREDIT_ALPHANUM4: {
            READER_CHECK(buffer_read_bytes(buffer, (uint8_t *) op->asset_code, 4))
            op->asset_code[4] = '\0';  // FIXME: it's OK?
            break;
        }
        case ASSET_TYPE_CREDIT_ALPHANUM12: {
            READER_CHECK(buffer_read_bytes(buffer, (uint8_t *) op->asset_code, 12))
            op->asset_code[12] = '\0';
            break;
        }
        default:
            return false;  // unknown asset type
    }

    return buffer_read32(buffer, &op->authorize);
}

bool read_account_merge(buffer_t *buffer, account_merge_op_t *op) {
    return read_muxed_account(buffer, &op->destination);
}

bool read_manage_data(buffer_t *buffer, manage_data_op_t *op) {
    size_t size;

    READER_CHECK(read_binary_string_ptr(buffer, (const uint8_t **) &op->data_name, &size, 64))
    op->data_name_size = size;

    bool has_value;
    READER_CHECK(buffer_read_bool(buffer, &has_value))
    if (has_value) {
        READER_CHECK(read_binary_string_ptr(buffer, (const uint8_t **) &op->data_value, &size, 64))
        op->data_value_size = size;
    } else {
        op->data_value_size = 0;
    }
    return true;
}

bool read_price(buffer_t *buffer, price_t *price) {
    READER_CHECK(buffer_read32(buffer, (uint32_t *) &price->n))
    READER_CHECK(buffer_read32(buffer, (uint32_t *) &price->d))

    // Denominator cannot be null, as it would lead to a division by zero.
    return price->d != 0;
}

bool read_manage_sell_offer(buffer_t *buffer, manage_sell_offer_op_t *op) {
    READER_CHECK(read_asset(buffer, &op->selling))
    READER_CHECK(read_asset(buffer, &op->buying))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->amount))
    READER_CHECK(read_price(buffer, &op->price))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->offer_id))
    return true;
}

bool read_manage_buy_offer(buffer_t *buffer, manage_buy_offer_op_t *op) {
    READER_CHECK(read_asset(buffer, &op->selling))
    READER_CHECK(read_asset(buffer, &op->buying))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->buy_amount))
    READER_CHECK(read_price(buffer, &op->price))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->offer_id))
    return true;
}

bool read_create_passive_sell_offer(buffer_t *buffer, create_passive_sell_offer_op_t *op) {
    READER_CHECK(read_asset(buffer, &op->selling))
    READER_CHECK(read_asset(buffer, &op->buying))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->amount))
    READER_CHECK(read_price(buffer, &op->price))
    return true;
}

bool read_change_trust(buffer_t *buffer, change_trust_op_t *op) {
    READER_CHECK(read_change_trust_asset(buffer, &op->line))
    return buffer_read64(buffer, &op->limit);
}

bool read_signer(buffer_t *buffer, signer_t *signer) {
    READER_CHECK(read_signer_key(buffer, &signer->key))
    READER_CHECK(buffer_read32(buffer, &signer->weight))
    return true;
}

bool read_set_options(buffer_t *buffer, set_options_op_t *set_options) {
    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) read_account_id,
                                    &set_options->inflation_destination,
                                    &set_options->inflation_destination_present))

    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) buffer_read32,
                                    &set_options->clear_flags,
                                    &set_options->clear_flags_present))
    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) buffer_read32,
                                    &set_options->set_flags,
                                    &set_options->set_flags_present))
    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) buffer_read32,
                                    &set_options->master_weight,
                                    &set_options->master_weight_present))
    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) buffer_read32,
                                    &set_options->low_threshold,
                                    &set_options->low_threshold_present))
    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) buffer_read32,
                                    &set_options->medium_threshold,
                                    &set_options->medium_threshold_present))
    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) buffer_read32,
                                    &set_options->high_threshold,
                                    &set_options->high_threshold_present))

    uint32_t home_domain_present;
    READER_CHECK(buffer_read32(buffer, &home_domain_present))
    set_options->home_domain_present = home_domain_present ? true : false;
    if (set_options->home_domain_present) {
        if (!buffer_read32(buffer, &set_options->home_domain_size) ||
            set_options->home_domain_size > HOME_DOMAIN_MAX_SIZE) {
            return false;
        }
        if (buffer->size - buffer->offset < num_bytes(set_options->home_domain_size)) {
            return false;
        }
        set_options->home_domain = buffer->ptr + buffer->offset;
        READER_CHECK(check_padding(set_options->home_domain,
                                   set_options->home_domain_size,
                                   num_bytes(set_options->home_domain_size)))  // security check
        buffer->offset += num_bytes(set_options->home_domain_size);
    } else {
        set_options->home_domain_size = 0;
    }

    return read_optional_type(buffer,
                              (xdr_type_reader) read_signer,
                              &set_options->signer,
                              &set_options->signer_present);
}

bool read_bump_sequence(buffer_t *buffer, bump_sequence_op_t *op) {
    return buffer_read64(buffer, (uint64_t *) &op->bump_to);
}

bool read_path_payment_strict_send(buffer_t *buffer, path_payment_strict_send_op_t *op) {
    uint32_t path_len;

    READER_CHECK(read_asset(buffer, &op->send_asset))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->send_amount))
    READER_CHECK(read_muxed_account(buffer, &op->destination))
    READER_CHECK(read_asset(buffer, &op->dest_asset))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->dest_min))
    READER_CHECK(buffer_read32(buffer, (uint32_t *) &path_len))
    op->path_len = path_len;
    if (op->path_len > 5) {
        return false;
    }
    for (int i = 0; i < op->path_len; i++) {
        READER_CHECK(read_asset(buffer, &op->path[i]))
    }
    return true;
}

bool read_claimant_predicate(buffer_t *buffer) {
    // Currently, does not support displaying claimant details.
    // So here we will not store the parsed data, just to ensure that the data can be parsed
    // correctly.
    uint32_t claim_predicate_type;
    uint32_t predicates_len;
    bool not_predicate_present;
    int64_t abs_before;
    int64_t rel_before;
    READER_CHECK(buffer_read32(buffer, &claim_predicate_type))
    switch (claim_predicate_type) {
        case CLAIM_PREDICATE_UNCONDITIONAL:
            return true;
        case CLAIM_PREDICATE_AND:
        case CLAIM_PREDICATE_OR:
            READER_CHECK(buffer_read32(buffer, &predicates_len))
            if (predicates_len != 2) {
                return false;
            }
            READER_CHECK(read_claimant_predicate(buffer))
            READER_CHECK(read_claimant_predicate(buffer))
            return true;
        case CLAIM_PREDICATE_NOT:
            READER_CHECK(buffer_read_bool(buffer, &not_predicate_present))
            if (not_predicate_present) {
                READER_CHECK(read_claimant_predicate(buffer))
            }
            return true;
        case CLAIM_PREDICATE_BEFORE_ABSOLUTE_TIME:
            READER_CHECK(buffer_read64(buffer, (uint64_t *) &abs_before))
            return true;
        case CLAIM_PREDICATE_BEFORE_RELATIVE_TIME:
            READER_CHECK(buffer_read64(buffer, (uint64_t *) &rel_before))
            return true;
        default:
            return false;
    }
}

bool read_claimant(buffer_t *buffer, claimant_t *claimant) {
    uint32_t claimant_type;
    READER_CHECK(buffer_read32(buffer, &claimant_type))
    claimant->type = claimant_type;

    switch (claimant->type) {
        case CLAIMANT_TYPE_V0:
            READER_CHECK(read_account_id(buffer, &claimant->v0.destination))
            READER_CHECK(read_claimant_predicate(buffer))
            return true;
        default:
            return false;
    }
}

bool read_create_claimable_balance(buffer_t *buffer, create_claimable_balance_op_t *op) {
    uint32_t claimant_len;
    READER_CHECK(read_asset(buffer, &op->asset))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->amount))
    READER_CHECK(buffer_read32(buffer, (uint32_t *) &claimant_len))
    if (claimant_len > 10) {
        return false;
    }
    op->claimant_len = claimant_len;
    for (int i = 0; i < op->claimant_len; i++) {
        READER_CHECK(read_claimant(buffer, &op->claimants[i]))
    }
    return true;
}
bool read_claimable_balance_id(buffer_t *buffer, claimable_balance_id *claimable_balance_id) {
    uint32_t claimable_balance_id_type;
    READER_CHECK(buffer_read32(buffer, &claimable_balance_id_type))
    claimable_balance_id->type = claimable_balance_id_type;

    switch (claimable_balance_id->type) {
        case CLAIMABLE_BALANCE_ID_TYPE_V0:
            READER_CHECK(buffer_read_bytes(buffer, claimable_balance_id->v0, 32))
            return true;
        default:
            return false;
    }
}

bool read_claim_claimable_balance(buffer_t *buffer, claim_claimable_balance_op_t *op) {
    READER_CHECK(read_claimable_balance_id(buffer, &op->balance_id))
    return true;
}

bool read_begin_sponsoring_future_reserves(buffer_t *buffer,
                                           begin_sponsoring_future_reserves_op_t *op) {
    READER_CHECK(read_account_id(buffer, &op->sponsored_id))
    return true;
}

bool read_ledger_key(buffer_t *buffer, ledger_key_t *ledger_key) {
    uint32_t ledger_entry_type;
    READER_CHECK(buffer_read32(buffer, &ledger_entry_type))
    ledger_key->type = ledger_entry_type;
    switch (ledger_key->type) {
        case ACCOUNT:
            READER_CHECK(read_account_id(buffer, &ledger_key->account.account_id))
            return true;
        case TRUSTLINE:
            READER_CHECK(read_account_id(buffer, &ledger_key->trust_line.account_id))
            READER_CHECK(read_trust_line_asset(buffer, &ledger_key->trust_line.asset))
            return true;
        case OFFER:
            READER_CHECK(read_account_id(buffer, &ledger_key->offer.seller_id))
            READER_CHECK(buffer_read64(buffer, (uint64_t *) &ledger_key->offer.offer_id))
            return true;
        case DATA:
            READER_CHECK(read_account_id(buffer, &ledger_key->data.account_id))
            READER_CHECK(read_binary_string_ptr(buffer,
                                                (const uint8_t **) &ledger_key->data.data_name,
                                                (size_t *) &ledger_key->data.data_name_size,
                                                64))
            return true;
        case CLAIMABLE_BALANCE:
            READER_CHECK(
                read_claimable_balance_id(buffer, &ledger_key->claimable_balance.balance_id))
            return true;
        case LIQUIDITY_POOL:
            READER_CHECK(buffer_read_bytes(buffer,
                                           ledger_key->liquidity_pool.liquidity_pool_id,
                                           LIQUIDITY_POOL_ID_SIZE))
            return true;
        default:
            return false;
    }
}

bool read_revoke_sponsorship(buffer_t *buffer, revoke_sponsorship_op_t *op) {
    uint32_t revoke_sponsorship_type;
    READER_CHECK(buffer_read32(buffer, &revoke_sponsorship_type))
    op->type = revoke_sponsorship_type;

    switch (op->type) {
        case REVOKE_SPONSORSHIP_LEDGER_ENTRY:
            READER_CHECK(read_ledger_key(buffer, &op->ledger_key))
            return true;
        case REVOKE_SPONSORSHIP_SIGNER:
            READER_CHECK(read_account_id(buffer, &op->signer.account_id))
            READER_CHECK(read_signer_key(buffer, &op->signer.signer_key))
            return true;
        default:
            return false;
    }
}

bool read_clawback(buffer_t *buffer, clawback_op_t *op) {
    READER_CHECK(read_asset(buffer, &op->asset))
    READER_CHECK(read_muxed_account(buffer, &op->from))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->amount))
    return true;
}

bool read_clawback_claimable_balance(buffer_t *buffer, clawback_claimable_balance_op_t *op) {
    READER_CHECK(read_claimable_balance_id(buffer, &op->balance_id))
    return true;
}

bool read_set_trust_line_flags(buffer_t *buffer, set_trust_line_flags_op_t *op) {
    READER_CHECK(read_account_id(buffer, &op->trustor))
    READER_CHECK(read_asset(buffer, &op->asset))
    READER_CHECK(buffer_read32(buffer, &op->clear_flags))
    READER_CHECK(buffer_read32(buffer, &op->set_flags))
    return true;
}

bool read_liquidity_pool_deposit(buffer_t *buffer, liquidity_pool_deposit_op_t *op) {
    READER_CHECK(buffer_read_bytes(buffer, op->liquidity_pool_id, LIQUIDITY_POOL_ID_SIZE))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->max_amount_a))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->max_amount_b))
    READER_CHECK(read_price(buffer, &op->min_price))
    READER_CHECK(read_price(buffer, &op->max_price))
    return true;
}

bool read_liquidity_pool_withdraw(buffer_t *buffer, liquidity_pool_withdraw_op_t *op) {
    READER_CHECK(buffer_read_bytes(buffer, op->liquidity_pool_id, LIQUIDITY_POOL_ID_SIZE))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->amount))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->min_amount_a))
    READER_CHECK(buffer_read64(buffer, (uint64_t *) &op->min_amount_b))
    return true;
}

bool read_operation(buffer_t *buffer, operation_t *operation) {
    explicit_bzero(operation, sizeof(operation_t));
    uint32_t op_type;

    READER_CHECK(read_optional_type(buffer,
                                    (xdr_type_reader) read_muxed_account,
                                    &operation->source_account,
                                    &operation->source_account_present))

    READER_CHECK(buffer_read32(buffer, &op_type))
    operation->type = op_type;
    switch (operation->type) {
        case OPERATION_TYPE_CREATE_ACCOUNT: {
            return read_create_account(buffer, &operation->create_account_op);
        }
        case OPERATION_TYPE_PAYMENT: {
            return read_payment(buffer, &operation->payment_op);
        }
        case OPERATION_TYPE_PATH_PAYMENT_STRICT_RECEIVE: {
            return read_path_payment_strict_receive(buffer,
                                                    &operation->path_payment_strict_receive_op);
        }
        case OPERATION_TYPE_CREATE_PASSIVE_SELL_OFFER: {
            return read_create_passive_sell_offer(buffer, &operation->create_passive_sell_offer_op);
        }
        case OPERATION_TYPE_MANAGE_SELL_OFFER: {
            return read_manage_sell_offer(buffer, &operation->manage_sell_offer_op);
        }
        case OPERATION_TYPE_SET_OPTIONS: {
            return read_set_options(buffer, &operation->set_options_op);
        }
        case OPERATION_TYPE_CHANGE_TRUST: {
            return read_change_trust(buffer, &operation->change_trust_op);
        }
        case OPERATION_TYPE_ALLOW_TRUST: {
            return read_allow_trust(buffer, &operation->allow_trust_op);
        }
        case OPERATION_TYPE_ACCOUNT_MERGE: {
            return read_account_merge(buffer, &operation->account_merge_op);
        }
        case OPERATION_TYPE_INFLATION: {
            return true;
        }
        case OPERATION_TYPE_MANAGE_DATA: {
            return read_manage_data(buffer, &operation->manage_data_op);
        }
        case OPERATION_TYPE_BUMP_SEQUENCE: {
            return read_bump_sequence(buffer, &operation->bump_sequence_op);
        }
        case OPERATION_TYPE_MANAGE_BUY_OFFER: {
            return read_manage_buy_offer(buffer, &operation->manage_buy_offer_op);
        }
        case OPERATION_TYPE_PATH_PAYMENT_STRICT_SEND: {
            return read_path_payment_strict_send(buffer, &operation->path_payment_strict_send_op);
        }
        case OPERATION_TYPE_CREATE_CLAIMABLE_BALANCE: {
            return read_create_claimable_balance(buffer, &operation->create_claimable_balance_op);
        }
        case OPERATION_TYPE_CLAIM_CLAIMABLE_BALANCE: {
            return read_claim_claimable_balance(buffer, &operation->claim_claimable_balance_op);
        }
        case OPERATION_TYPE_BEGIN_SPONSORING_FUTURE_RESERVES: {
            return read_begin_sponsoring_future_reserves(
                buffer,
                &operation->begin_sponsoring_future_reserves_op);
        }
        case OPERATION_TYPE_END_SPONSORING_FUTURE_RESERVES: {
            return true;
        }
        case OPERATION_TYPE_REVOKE_SPONSORSHIP: {
            return read_revoke_sponsorship(buffer, &operation->revoke_sponsorship_op);
        }
        case OPERATION_TYPE_CLAWBACK: {
            return read_clawback(buffer, &operation->clawback_op);
        }
        case OPERATION_TYPE_CLAWBACK_CLAIMABLE_BALANCE: {
            return read_clawback_claimable_balance(buffer,
                                                   &operation->clawback_claimable_balance_op);
        }
        case OPERATION_TYPE_SET_TRUST_LINE_FLAGS: {
            return read_set_trust_line_flags(buffer, &operation->set_trust_line_flags_op);
        }
        case OPERATION_TYPE_LIQUIDITY_POOL_DEPOSIT:
            return read_liquidity_pool_deposit(buffer, &operation->liquidity_pool_deposit_op);
        case OPERATION_TYPE_LIQUIDITY_POOL_WITHDRAW:
            return read_liquidity_pool_withdraw(buffer, &operation->liquidity_pool_withdraw_op);
        default:
            return false;
    }
    return false;
}

bool read_transaction_source(buffer_t *buffer, muxed_account_t *source) {
    return read_muxed_account(buffer, source);
}

bool read_transaction_fee(buffer_t *buffer, uint32_t *fee) {
    return buffer_read32(buffer, fee);
}

bool read_transaction_sequence(buffer_t *buffer, sequence_number_t *sequence_number) {
    return buffer_read64(buffer, (uint64_t *) sequence_number);
}

bool read_transaction_preconditions(buffer_t *buffer, preconditions_t *preconditions) {
    return read_preconditions(buffer, preconditions);
}

bool read_transaction_memo(buffer_t *buffer, memo_t *memo) {
    return read_memo(buffer, memo);
}

bool read_transaction_operation_len(buffer_t *buffer, uint8_t *operations_len) {
    uint32_t len;
    READER_CHECK(buffer_read32(buffer, &len))
    if (len > MAX_OPS) {
        return false;
    }
    *operations_len = len;
    return true;
}

bool read_transaction_details(buffer_t *buffer, transaction_details_t *transaction) {
    // account used to run the (inner)transaction
    READER_CHECK(read_transaction_source(buffer, &transaction->source_account))

    // the fee the source_account will pay
    READER_CHECK(read_transaction_fee(buffer, &transaction->fee))

    // sequence number to consume in the account
    READER_CHECK(read_transaction_sequence(buffer, &transaction->sequence_number))

    // validity conditions
    READER_CHECK(read_transaction_preconditions(buffer, &transaction->cond))

    READER_CHECK(read_transaction_memo(buffer, &transaction->memo))
    READER_CHECK(read_transaction_operation_len(buffer, &transaction->operations_len))
    return true;
}

bool read_transaction_ext(buffer_t *buffer) {
    uint32_t ext;
    READER_CHECK(buffer_read32(buffer, &ext))
    if (ext != 0) {
        return false;
    }
    return true;
}

bool read_decorated_signature_len(buffer_t *buffer, uint8_t *len) {
    uint32_t slen;
    READER_CHECK(buffer_read32(buffer, &slen))
    *len = slen;
    return true;
}

bool read_decorated_signature(buffer_t *buffer, decorated_signature_t *decorated_signature) {
    READER_CHECK(buffer_read_bytes(buffer, decorated_signature->signature_hint, 4))
    uint32_t signature_len;
    READER_CHECK(buffer_read32(buffer, &signature_len));
    decorated_signature->signature_size = signature_len;
    READER_CHECK(buffer_read_bytes(buffer, decorated_signature->signature, signature_len))
    return true;
}

bool read_fee_bump_transaction_fee_source(buffer_t *buffer, muxed_account_t *fee_source) {
    return read_muxed_account(buffer, fee_source);
}

bool read_fee_bump_transaction_fee(buffer_t *buffer, int64_t *fee) {
    return buffer_read64(buffer, (uint64_t *) fee);
}

bool read_fee_bump_transaction_details(buffer_t *buffer,
                                       fee_bump_transaction_details_t *fee_bump_transaction) {
    READER_CHECK(read_fee_bump_transaction_fee_source(buffer, &fee_bump_transaction->fee_source))
    READER_CHECK(read_fee_bump_transaction_fee(buffer, &fee_bump_transaction->fee))
    return true;
}

bool read_fee_bump_transaction_ext(buffer_t *buffer) {
    uint32_t ext;
    READER_CHECK(buffer_read32(buffer, &ext))
    if (ext != 0) {
        return false;
    }
    return true;
}

bool read_transaction_envelope_type(buffer_t *buffer, envelope_type_t *envelope_type) {
    uint32_t type;
    READER_CHECK(buffer_read32(buffer, &type))
    if (type != ENVELOPE_TYPE_TX && type != ENVELOPE_TYPE_TX_FEE_BUMP) {
        return false;
    }

    *envelope_type = type;
    return true;
}

static bool read_network(buffer_t *buffer, uint8_t *network) {
    READER_CHECK(buffer_can_read(buffer, HASH_SIZE))
    if (memcmp(buffer->ptr, NETWORK_ID_PUBLIC_HASH, HASH_SIZE) == 0) {
        *network = NETWORK_TYPE_PUBLIC;
    } else if (memcmp(buffer->ptr, NETWORK_ID_TEST_HASH, HASH_SIZE) == 0) {
        *network = NETWORK_TYPE_TEST;
    } else {
        *network = NETWORK_TYPE_UNKNOWN;
    }
    buffer_advance(buffer, HASH_SIZE);
    return true;
}

bool parse_tx_xdr(const uint8_t *data, size_t size, tx_ctx_t *txCtx) {
    buffer_t buffer = {data, size, 0};
    uint32_t envelope_type;

    uint16_t offset = txCtx->offset;
    buffer.offset = txCtx->offset;

    if (offset == 0) {
        explicit_bzero(&txCtx->tx_details, sizeof(transaction_details_t));
        explicit_bzero(&txCtx->fee_bump_tx_details, sizeof(fee_bump_transaction_details_t));
        READER_CHECK(read_network(&buffer, &txCtx->network))
        READER_CHECK(buffer_read32(&buffer, &envelope_type))
        txCtx->envelope_type = envelope_type;
        switch (envelope_type) {
            case ENVELOPE_TYPE_TX:
                READER_CHECK(read_transaction_details(&buffer, &txCtx->tx_details))
                break;
            case ENVELOPE_TYPE_TX_FEE_BUMP:
                READER_CHECK(
                    read_fee_bump_transaction_details(&buffer, &txCtx->fee_bump_tx_details))
                uint32_t inner_envelope_type;
                READER_CHECK(buffer_read32(&buffer, &inner_envelope_type))
                if (inner_envelope_type != ENVELOPE_TYPE_TX) {
                    return false;
                }
                READER_CHECK(read_transaction_details(&buffer, &txCtx->tx_details))
                break;
            default:
                return false;
        }
    }

    READER_CHECK(read_operation(&buffer, &txCtx->tx_details.op_details))
    offset = buffer.offset;
    txCtx->tx_details.operation_index += 1;
    txCtx->offset = offset;
    return true;
}
