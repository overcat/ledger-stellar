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

#include <stdbool.h>  // bool
#include <string.h>   // memset
#include "globals.h"
#include "common/format.h"
#include "utils.h"
#include "types.h"
#include "transaction/transaction_parser.h"
#include "transaction_formatter.h"

#ifdef TEST
uint8_t G_ui_current_data_index;
char G_ui_detail_caption[DETAIL_CAPTION_MAX_LENGTH];
char G_ui_detail_value[DETAIL_VALUE_MAX_LENGTH];
#endif  // TEST

char op_caption[OPERATION_CAPTION_MAX_SIZE];
format_function_t formatter_stack[MAX_FORMATTERS_PER_OPERATION];
int8_t formatter_index;

static void push_to_formatter_stack(format_function_t formatter) {
    if (formatter_index + 1 >= MAX_FORMATTERS_PER_OPERATION) {
        THROW(0x6124);
    }
    formatter_stack[formatter_index + 1] = formatter;
}

static void format_next_step(tx_ctx_t *txCtx) {
    (void) txCtx;
    formatter_stack[formatter_index] = NULL;
    set_state_data(true);
}

static void format_transaction_source(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Tx Source");
    // TODO: fix G_context.raw_public_key
    if (txCtx->envelope_type == ENVELOPE_TYPE_TX &&
        txCtx->tx_details.source_account.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->tx_details.source_account.ed25519,
               G_context.raw_public_key,
               RAW_ED25519_PUBLIC_KEY_SIZE) == 0) {
        print_muxed_account(&txCtx->tx_details.source_account,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            6,
                            6);
    } else {
        print_muxed_account(&txCtx->tx_details.source_account,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            0,
                            0);
    }
    push_to_formatter_stack(format_next_step);
}

static void format_min_seq_ledger_gap(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Min Seq Ledger Gap");
    print_uint(txCtx->tx_details.cond.min_seq_ledger_gap,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_transaction_source);
}

static void format_min_seq_ledger_gap_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.cond.min_seq_ledger_gap == 0) {
        format_transaction_source(txCtx);
    } else {
        format_min_seq_ledger_gap(txCtx);
    }
}

static void format_min_seq_age(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Min Seq Age");
    print_uint(txCtx->tx_details.cond.min_seq_age, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_min_seq_ledger_gap_prepare);
}

static void format_min_seq_age_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.cond.min_seq_age == 0) {
        format_min_seq_ledger_gap_prepare(txCtx);
    } else {
        format_min_seq_age(txCtx);
    }
}

static void format_min_seq_num(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Min Seq Num");
    print_uint(txCtx->tx_details.cond.min_seq_num, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_min_seq_age_prepare);
}

static void format_min_seq_num_prepare(tx_ctx_t *txCtx) {
    if (!txCtx->tx_details.cond.min_seq_num_present || txCtx->tx_details.cond.min_seq_num == 0) {
        format_min_seq_age_prepare(txCtx);
    } else {
        format_min_seq_num(txCtx);
    }
}

static void format_ledger_bounds_max_ledger(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Ledger Bounds Max");
    print_uint(txCtx->tx_details.cond.ledger_bounds.max_ledger,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_min_seq_num_prepare);
}

static void format_ledger_bounds_min_ledger(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Ledger Bounds Min");
    print_uint(txCtx->tx_details.cond.ledger_bounds.min_ledger,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    if (txCtx->tx_details.cond.ledger_bounds.max_ledger != 0) {
        push_to_formatter_stack(&format_ledger_bounds_max_ledger);
    } else {
        push_to_formatter_stack(&format_min_seq_num_prepare);
    }
}

static void format_ledger_bounds(tx_ctx_t *txCtx) {
    if (!txCtx->tx_details.cond.ledger_bounds_present ||
        (txCtx->tx_details.cond.ledger_bounds.min_ledger == 0 &&
         txCtx->tx_details.cond.ledger_bounds.max_ledger == 0)) {
        format_min_seq_num_prepare(txCtx);
    } else if (txCtx->tx_details.cond.ledger_bounds.min_ledger != 0) {
        format_ledger_bounds_min_ledger(txCtx);
    } else {
        format_ledger_bounds_max_ledger(txCtx);
    }
}

static void format_time_bounds_max_time(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Valid Before (UTC)");
    if (!print_time(txCtx->tx_details.cond.time_bounds.max_time,
                    G_ui_detail_value,
                    DETAIL_VALUE_MAX_LENGTH)) {
        THROW(0x6126);
    };
    push_to_formatter_stack(&format_ledger_bounds);
}

static void format_time_bounds_min_time(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Valid After (UTC)");
    if (!print_time(txCtx->tx_details.cond.time_bounds.min_time,
                    G_ui_detail_value,
                    DETAIL_VALUE_MAX_LENGTH)) {
        THROW(0x6126);
    };

    if (txCtx->tx_details.cond.time_bounds.max_time != 0) {
        push_to_formatter_stack(&format_time_bounds_max_time);
    } else {
        push_to_formatter_stack(&format_ledger_bounds);
    }
}

static void format_time_bounds(tx_ctx_t *txCtx) {
    if (!txCtx->tx_details.cond.time_bounds_present ||
        (txCtx->tx_details.cond.time_bounds.min_time == 0 &&
         txCtx->tx_details.cond.time_bounds.max_time == 0)) {
        format_ledger_bounds(txCtx);
    } else if (txCtx->tx_details.cond.time_bounds.min_time != 0) {
        format_time_bounds_min_time(txCtx);
    } else {
        format_time_bounds_max_time(txCtx);
    }
}

static void format_sequence(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Sequence Num");
    print_uint(txCtx->tx_details.sequence_number, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_time_bounds);
}

static void format_fee(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Max Fee");
    asset_t asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->tx_details.fee,
                 &asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
#ifdef TEST
    push_to_formatter_stack(&format_sequence);
#else
    if (HAS_SETTING(S_SEQUENCE_NUMBER_ENABLED)) {
        push_to_formatter_stack(&format_sequence);
    } else {
        push_to_formatter_stack(&format_time_bounds);
    }
#endif  // TEST
}

static void format_memo(tx_ctx_t *txCtx) {
    memo_t *memo = &txCtx->tx_details.memo;
    switch (memo->type) {
        case MEMO_ID: {
            strcpy(G_ui_detail_caption, "Memo ID");
            print_uint(memo->id, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
            break;
        }
        case MEMO_TEXT: {
            if (is_printable_string(memo->text.text, memo->text.text_size)) {
                strcpy(G_ui_detail_caption, "Memo Text");
                strlcpy(G_ui_detail_value, (char *) memo->text.text, MEMO_TEXT_MAX_SIZE + 1);
            } else {
                strcpy(G_ui_detail_caption, "Memo Text (base64)");
                base64_encode(memo->text.text,
                              memo->text.text_size,
                              G_ui_detail_value,
                              DETAIL_VALUE_MAX_LENGTH);
            }
            break;
        }
        case MEMO_HASH: {
            strcpy(G_ui_detail_caption, "Memo Hash");
            print_binary(memo->hash, HASH_SIZE, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 6, 6);
            break;
        }
        case MEMO_RETURN: {
            strcpy(G_ui_detail_caption, "Memo Return");
            print_binary(memo->hash, HASH_SIZE, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 6, 6);
            break;
        }
        default: {
            // TODO: remove the branch
            strcpy(G_ui_detail_caption, "Memo");
            strcpy(G_ui_detail_value, "[none]");
        }
    }
    push_to_formatter_stack(&format_fee);
}

static void format_transaction_details(tx_ctx_t *txCtx) {
    switch (txCtx->envelope_type) {
        case ENVELOPE_TYPE_TX_FEE_BUMP:
            strcpy(G_ui_detail_caption, "InnerTx");
            break;
        case ENVELOPE_TYPE_TX:
            strcpy(G_ui_detail_caption, "Transaction");
            break;
    }
    strcpy(G_ui_detail_value, "Details");
    if (txCtx->tx_details.memo.type != MEMO_NONE) {
        push_to_formatter_stack(&format_memo);
    } else {
        push_to_formatter_stack(&format_fee);
    }
}

static void format_operation_source(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Op Source");
    if (txCtx->envelope_type == ENVELOPE_TYPE_TX &&
        txCtx->tx_details.source_account.type == KEY_TYPE_ED25519 &&
        txCtx->tx_details.op_details.source_account.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->tx_details.source_account.ed25519,
               G_context.raw_public_key,
               RAW_ED25519_PUBLIC_KEY_SIZE) == 0 &&
        memcmp(txCtx->tx_details.op_details.source_account.ed25519,
               G_context.raw_public_key,
               RAW_ED25519_PUBLIC_KEY_SIZE) == 0) {
        print_muxed_account(&txCtx->tx_details.op_details.source_account,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            6,
                            6);
    } else {
        print_muxed_account(&txCtx->tx_details.op_details.source_account,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            0,
                            0);
    }

    if (txCtx->tx_details.operation_index == txCtx->tx_details.operations_len) {
        // last operation
        push_to_formatter_stack(NULL);
    } else {
        // more operations
        push_to_formatter_stack(&format_next_step);
    }
}

static void format_operation_source_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.source_account_present) {
        // If the source exists, when the user clicks the next button,
        // it will jump to the page showing the source
        push_to_formatter_stack(&format_operation_source);
    } else {
        // If not, jump to the signing page or show the next operation.
        if (txCtx->tx_details.operation_index == txCtx->tx_details.operations_len) {
            // last operation
            push_to_formatter_stack(NULL);
        } else {
            // more operations
            push_to_formatter_stack(&format_next_step);
        }
    }
}

static void format_bump_sequence_bump_to(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Bump To");
    print_int(txCtx->tx_details.op_details.bump_sequence_op.bump_to,
              G_ui_detail_value,
              DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_bump_sequence(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Bump Sequence");
    push_to_formatter_stack(&format_bump_sequence_bump_to);
}

static void format_inflation(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Inflation");
    format_operation_source_prepare(txCtx);
}

static void format_account_merge_destination(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Destination");
    print_muxed_account(&txCtx->tx_details.op_details.account_merge_op.destination,
                        G_ui_detail_value,
                        DETAIL_VALUE_MAX_LENGTH,
                        0,
                        0);
    format_operation_source_prepare(txCtx);
}

static void format_account_merge_detail(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Merge Account");
    if (txCtx->tx_details.op_details.source_account_present) {
        print_muxed_account(&txCtx->tx_details.op_details.source_account,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            0,
                            0);
    } else {
        print_muxed_account(&txCtx->tx_details.source_account,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            0,
                            0);
    }
    push_to_formatter_stack(&format_account_merge_destination);
}

static void format_account_merge(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Account Merge");
    push_to_formatter_stack(&format_account_merge_detail);
}

static void format_manage_data_value(tx_ctx_t *txCtx) {
    char tmp[89];
    if (is_printable_string(txCtx->tx_details.op_details.manage_data_op.data_value,
                            txCtx->tx_details.op_details.manage_data_op.data_value_size)) {
        strcpy(G_ui_detail_caption, "Data Value");
        strcpy(tmp, (char *) txCtx->tx_details.op_details.manage_data_op.data_value);
        tmp[txCtx->tx_details.op_details.manage_data_op.data_value_size] = '\0';
        strcpy(G_ui_detail_value, tmp);
    } else {
        strcpy(G_ui_detail_caption, "Data Value (base64)");
        base64_encode(txCtx->tx_details.op_details.manage_data_op.data_value,
                      txCtx->tx_details.op_details.manage_data_op.data_value_size,
                      tmp,
                      sizeof(tmp));
        print_summary(tmp, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 12, 12);
    }
    format_operation_source_prepare(txCtx);
}

static void format_manage_data_detail(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.manage_data_op.data_value_size) {
        strcpy(G_ui_detail_caption, "Set Data");
        push_to_formatter_stack(&format_manage_data_value);
    } else {
        strcpy(G_ui_detail_caption, "Remove Data");
        format_operation_source_prepare(txCtx);
    }
    char tmp[65];
    memcpy(tmp,
           txCtx->tx_details.op_details.manage_data_op.data_name,
           txCtx->tx_details.op_details.manage_data_op.data_name_size);
    tmp[txCtx->tx_details.op_details.manage_data_op.data_name_size] = '\0';
    print_summary(tmp, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 12, 12);
}

static void format_manage_data(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Manage Data");
    push_to_formatter_stack(&format_manage_data_detail);
}

static void format_allow_trust_authorize(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Authorize Flag");
    if (txCtx->tx_details.op_details.allow_trust_op.authorize == AUTHORIZED_FLAG) {
        strlcpy(G_ui_detail_value, "AUTHORIZED_FLAG", DETAIL_VALUE_MAX_LENGTH);
    } else if (txCtx->tx_details.op_details.allow_trust_op.authorize ==
               AUTHORIZED_TO_MAINTAIN_LIABILITIES_FLAG) {
        strlcpy(G_ui_detail_value,
                "AUTHORIZED_TO_MAINTAIN_LIABILITIES_FLAG",
                DETAIL_VALUE_MAX_LENGTH);
    } else {
        strlcpy(G_ui_detail_value, "UNAUTHORIZED_FLAG", DETAIL_VALUE_MAX_LENGTH);
    }
    format_operation_source_prepare(txCtx);
}

static void format_allow_trust_asset_code(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Asset Code");
    strlcpy(G_ui_detail_value,
            txCtx->tx_details.op_details.allow_trust_op.asset_code,
            DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_allow_trust_authorize);
}

static void format_allow_trust_trustor(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Trustor");
    print_account_id(txCtx->tx_details.op_details.allow_trust_op.trustor,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    push_to_formatter_stack(&format_allow_trust_asset_code);
}

static void format_allow_trust(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Allow Trust");
    push_to_formatter_stack(&format_allow_trust_trustor);
}

static void format_set_option_signer_weight(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Weight");
    print_uint(txCtx->tx_details.op_details.set_options_op.signer.weight,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_set_option_signer_detail(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Signer Key");
    signer_key_t *key = &txCtx->tx_details.op_details.set_options_op.signer.key;

    switch (key->type) {
        case SIGNER_KEY_TYPE_ED25519: {
            print_account_id(key->ed25519, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 0, 0);
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            char tmp[57];
            encode_hash_x_key(key->hash_x, tmp, DETAIL_VALUE_MAX_LENGTH);
            print_summary(tmp, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 12, 12);
            break;
        }

        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            char tmp[57];
            encode_pre_auth_x_key(key->pre_auth_tx, tmp, DETAIL_VALUE_MAX_LENGTH);
            print_summary(tmp, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 12, 12);
            break;
        }
        default:
            break;
    }
    push_to_formatter_stack(&format_set_option_signer_weight);
}

static void format_set_option_signer(tx_ctx_t *txCtx) {
    signer_t *signer = &txCtx->tx_details.op_details.set_options_op.signer;
    if (signer->weight) {
        strcpy(G_ui_detail_caption, "Add Signer");
    } else {
        strcpy(G_ui_detail_caption, "Remove Signer");
    }
    switch (signer->key.type) {
        case SIGNER_KEY_TYPE_ED25519: {
            strcpy(G_ui_detail_value, "Type Public Key");
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            strcpy(G_ui_detail_value, "Type Hash(x)");
            break;
        }
        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            strcpy(G_ui_detail_value, "Type Pre-Auth");
            break;
        }
        default:
            break;
    }
    push_to_formatter_stack(&format_set_option_signer_detail);
}

static void format_set_option_signer_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.signer_present) {
        push_to_formatter_stack(&format_set_option_signer);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_set_option_home_domain(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Home Domain");
    if (txCtx->tx_details.op_details.set_options_op.home_domain_size) {
        memcpy(G_ui_detail_value,
               txCtx->tx_details.op_details.set_options_op.home_domain,
               txCtx->tx_details.op_details.set_options_op.home_domain_size);
        G_ui_detail_value[txCtx->tx_details.op_details.set_options_op.home_domain_size] = '\0';
    } else {
        strcpy(G_ui_detail_value, "[remove home domain from account]");
    }
    format_set_option_signer_prepare(txCtx);
}

static void format_set_option_home_domain_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.home_domain_present) {
        push_to_formatter_stack(&format_set_option_home_domain);
    } else {
        format_set_option_signer_prepare(txCtx);
    }
}

static void format_set_option_high_threshold(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "High Threshold");
    print_uint(txCtx->tx_details.op_details.set_options_op.high_threshold,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    format_set_option_home_domain_prepare(txCtx);
}

static void format_set_option_high_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.high_threshold_present) {
        push_to_formatter_stack(&format_set_option_high_threshold);
    } else {
        format_set_option_home_domain_prepare(txCtx);
    }
}

static void format_set_option_medium_threshold(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Medium Threshold");
    print_uint(txCtx->tx_details.op_details.set_options_op.medium_threshold,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    format_set_option_high_threshold_prepare(txCtx);
}

static void format_set_option_medium_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.medium_threshold_present) {
        push_to_formatter_stack(&format_set_option_medium_threshold);
    } else {
        format_set_option_high_threshold_prepare(txCtx);
    }
}

static void format_set_option_low_threshold(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Low Threshold");
    print_uint(txCtx->tx_details.op_details.set_options_op.low_threshold,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    format_set_option_medium_threshold_prepare(txCtx);
}

static void format_set_option_low_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.low_threshold_present) {
        push_to_formatter_stack(&format_set_option_low_threshold);
    } else {
        format_set_option_medium_threshold_prepare(txCtx);
    }
}

static void format_set_option_master_weight(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Master Weight");
    print_uint(txCtx->tx_details.op_details.set_options_op.master_weight,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);
    format_set_option_low_threshold_prepare(txCtx);
}

static void format_set_option_master_weight_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.master_weight_present) {
        push_to_formatter_stack(&format_set_option_master_weight);
    } else {
        format_set_option_low_threshold_prepare(txCtx);
    }
}

static void format_set_option_set_flags(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Set Flags");
    print_flags(txCtx->tx_details.op_details.set_options_op.set_flags,
                G_ui_detail_value,
                DETAIL_VALUE_MAX_LENGTH);
    format_set_option_master_weight_prepare(txCtx);
}

static void format_set_option_set_flags_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.set_flags) {
        push_to_formatter_stack(&format_set_option_set_flags);
    } else {
        format_set_option_master_weight_prepare(txCtx);
    }
}

static void format_set_option_clear_flags(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Clear Flags");
    print_flags(txCtx->tx_details.op_details.set_options_op.clear_flags,
                G_ui_detail_value,
                DETAIL_VALUE_MAX_LENGTH);
    format_set_option_set_flags_prepare(txCtx);
}

static void format_set_option_clear_flags_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.clear_flags) {
        push_to_formatter_stack(&format_set_option_clear_flags);
    } else {
        format_set_option_set_flags_prepare(txCtx);
    }
}

static void format_set_option_inflation_destination(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Inflation Dest");
    print_account_id(txCtx->tx_details.op_details.set_options_op.inflation_destination,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    format_set_option_clear_flags_prepare(txCtx);
}

static void format_set_option_inflation_destination_prepare(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.set_options_op.inflation_destination_present) {
        push_to_formatter_stack(format_set_option_inflation_destination);
    } else {
        format_set_option_clear_flags_prepare(txCtx);
    }
}

static void format_set_options_empty_body(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "SET OPTIONS");
    strcpy(G_ui_detail_value, "BODY IS EMPTY");
    format_operation_source_prepare(txCtx);
}

static bool is_empty_set_options_body(tx_ctx_t *txCtx) {
    return !(txCtx->tx_details.op_details.set_options_op.inflation_destination_present ||
             txCtx->tx_details.op_details.set_options_op.clear_flags_present ||
             txCtx->tx_details.op_details.set_options_op.set_flags_present ||
             txCtx->tx_details.op_details.set_options_op.master_weight_present ||
             txCtx->tx_details.op_details.set_options_op.low_threshold_present ||
             txCtx->tx_details.op_details.set_options_op.medium_threshold_present ||
             txCtx->tx_details.op_details.set_options_op.high_threshold_present ||
             txCtx->tx_details.op_details.set_options_op.home_domain_present ||
             txCtx->tx_details.op_details.set_options_op.signer_present);
}

static void format_set_options(tx_ctx_t *txCtx) {
    // this operation is a special one among all operations, because all its fields are optional.
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Set Options");
    if (is_empty_set_options_body(txCtx)) {
        push_to_formatter_stack(format_set_options_empty_body);
    } else {
        format_set_option_inflation_destination_prepare(txCtx);
    }
}

static void format_change_trust_limit(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Trust Limit");
    print_amount(txCtx->tx_details.op_details.change_trust_op.limit,
                 NULL,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_change_trust_detail_liquidity_pool_fee(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Pool Fee Rate");
    uint64_t fee =
        ((uint64_t)
             txCtx->tx_details.op_details.change_trust_op.line.liquidity_pool.constant_product.fee *
         10000000) /
        100;
    print_amount(fee, NULL, txCtx->network, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    strlcat(G_ui_detail_value, "%", DETAIL_VALUE_MAX_LENGTH);
    if (txCtx->tx_details.op_details.change_trust_op.limit &&
        txCtx->tx_details.op_details.change_trust_op.limit != INT64_MAX) {
        push_to_formatter_stack(&format_change_trust_limit);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_change_trust_detail_liquidity_pool_asset_b(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Asset B");
    print_asset(
        &txCtx->tx_details.op_details.change_trust_op.line.liquidity_pool.constant_product.assetB,
        txCtx->network,
        G_ui_detail_value,
        DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_fee);
}

static void format_change_trust_detail_liquidity_pool_asset_a(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Asset A");
    print_asset(
        &txCtx->tx_details.op_details.change_trust_op.line.liquidity_pool.constant_product.assetA,
        txCtx->network,
        G_ui_detail_value,
        DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset_b);
}

static void format_change_trust_detail_liquidity_pool_asset(tx_ctx_t *txCtx) {
    (void) txCtx;

    strcpy(G_ui_detail_caption, "Liquidity Pool");
    strcpy(G_ui_detail_value, "Asset");
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset_a);
}

static void format_change_trust_detail(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.change_trust_op.limit) {
        strcpy(G_ui_detail_caption, "Change Trust");
    } else {
        strcpy(G_ui_detail_caption, "Remove Trust");
    }
    uint8_t asset_type = txCtx->tx_details.op_details.change_trust_op.line.type;
    switch (asset_type) {
        case ASSET_TYPE_CREDIT_ALPHANUM4:
        case ASSET_TYPE_CREDIT_ALPHANUM12:
            print_asset((asset_t *) &txCtx->tx_details.op_details.change_trust_op.line,
                        txCtx->network,
                        G_ui_detail_value,
                        DETAIL_VALUE_MAX_LENGTH);
            if (txCtx->tx_details.op_details.change_trust_op.limit &&
                txCtx->tx_details.op_details.change_trust_op.limit != INT64_MAX) {
                push_to_formatter_stack(&format_change_trust_limit);
            } else {
                format_operation_source_prepare(txCtx);
            }
            break;
        case ASSET_TYPE_POOL_SHARE:
            push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset);
            break;
        default:
            return;
    }
}

static void format_change_trust(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Change Trust");
    push_to_formatter_stack(&format_change_trust_detail);
}

static void format_manage_sell_offer_sell(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Sell");
    print_amount(txCtx->tx_details.op_details.manage_sell_offer_op.amount,
                 &txCtx->tx_details.op_details.manage_sell_offer_op.selling,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_manage_sell_offer_price(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Price");
    uint64_t price =
        ((uint64_t) txCtx->tx_details.op_details.manage_sell_offer_op.price.n * 10000000) /
        txCtx->tx_details.op_details.manage_sell_offer_op.price.d;
    print_amount(price,
                 &txCtx->tx_details.op_details.manage_sell_offer_op.buying,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_manage_sell_offer_sell);
}

static void format_manage_sell_offer_buy(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Buy");
    print_asset(&txCtx->tx_details.op_details.manage_sell_offer_op.buying,
                txCtx->network,
                G_ui_detail_value,
                DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_manage_sell_offer_price);
}

static void format_manage_sell_offer_detail(tx_ctx_t *txCtx) {
    if (!txCtx->tx_details.op_details.manage_sell_offer_op.amount) {
        strcpy(G_ui_detail_caption, "Remove Offer");
        print_uint(txCtx->tx_details.op_details.manage_sell_offer_op.offer_id,
                   G_ui_detail_value,
                   DETAIL_VALUE_MAX_LENGTH);
        format_operation_source_prepare(txCtx);
    } else {
        if (txCtx->tx_details.op_details.manage_sell_offer_op.offer_id) {
            strcpy(G_ui_detail_caption, "Change Offer");
            print_uint(txCtx->tx_details.op_details.manage_sell_offer_op.offer_id,
                       G_ui_detail_value,
                       DETAIL_VALUE_MAX_LENGTH);
        } else {
            strcpy(G_ui_detail_caption, "Create Offer");
            strcpy(G_ui_detail_value, "Type Active");
        }
        push_to_formatter_stack(&format_manage_sell_offer_buy);
    }
}

static void format_manage_sell_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Manage Sell Offer");
    push_to_formatter_stack(&format_manage_sell_offer_detail);
}

static void format_manage_buy_offer_buy(tx_ctx_t *txCtx) {
    manage_buy_offer_op_t *op = &txCtx->tx_details.op_details.manage_buy_offer_op;

    strcpy(G_ui_detail_caption, "Buy");
    print_amount(op->buy_amount,
                 &op->buying,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_manage_buy_offer_price(tx_ctx_t *txCtx) {
    manage_buy_offer_op_t *op = &txCtx->tx_details.op_details.manage_buy_offer_op;

    strcpy(G_ui_detail_caption, "Price");
    uint64_t price = ((uint64_t) op->price.n * 10000000) / op->price.d;
    print_amount(price, &op->selling, txCtx->network, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_manage_buy_offer_buy);
}

static void format_manage_buy_offer_sell(tx_ctx_t *txCtx) {
    manage_buy_offer_op_t *op = &txCtx->tx_details.op_details.manage_buy_offer_op;

    strcpy(G_ui_detail_caption, "Sell");
    print_asset(&op->selling, txCtx->network, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_manage_buy_offer_price);
}

static void format_manage_buy_offer_detail(tx_ctx_t *txCtx) {
    manage_buy_offer_op_t *op = &txCtx->tx_details.op_details.manage_buy_offer_op;

    if (op->buy_amount == 0) {
        strcpy(G_ui_detail_caption, "Remove Offer");
        print_uint(op->offer_id, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
        format_operation_source_prepare(txCtx);  // TODO
    } else {
        if (op->offer_id) {
            strcpy(G_ui_detail_caption, "Change Offer");
            print_uint(op->offer_id, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
        } else {
            strcpy(G_ui_detail_caption, "Create Offer");
            strcpy(G_ui_detail_value, "Type Active");
        }
        push_to_formatter_stack(&format_manage_buy_offer_sell);
    }
}

static void format_manage_buy_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Manage Buy Offer");
    push_to_formatter_stack(&format_manage_buy_offer_detail);
}

static void format_create_passive_sell_offer_sell(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Sell");
    print_amount(txCtx->tx_details.op_details.create_passive_sell_offer_op.amount,
                 &txCtx->tx_details.op_details.create_passive_sell_offer_op.selling,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_create_passive_sell_offer_price(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Price");

    create_passive_sell_offer_op_t *op = &txCtx->tx_details.op_details.create_passive_sell_offer_op;
    uint64_t price = ((uint64_t) op->price.n * 10000000) / op->price.d;
    print_amount(price, &op->buying, txCtx->network, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_create_passive_sell_offer_sell);
}

static void format_create_passive_sell_offer_buy(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Buy");
    print_asset(&txCtx->tx_details.op_details.create_passive_sell_offer_op.buying,
                txCtx->network,
                G_ui_detail_value,
                DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_create_passive_sell_offer_price);
}

static void format_create_passive_sell_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Create Passive Sell Offer");
    push_to_formatter_stack(&format_create_passive_sell_offer_buy);
}

static void format_path_payment_strict_receive_path_via(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Via");
    uint8_t i;
    for (i = 0; i < txCtx->tx_details.op_details.path_payment_strict_receive_op.path_len; i++) {
        char asset_name[12 + 1];
        asset_t *asset = &txCtx->tx_details.op_details.path_payment_strict_receive_op.path[i];
        if (strlen(G_ui_detail_value) != 0) {
            strlcat(G_ui_detail_value, ", ", DETAIL_VALUE_MAX_LENGTH);
        }
        print_asset(asset, txCtx->network, asset_name, sizeof(asset_name));
        strlcat(G_ui_detail_value, asset_name, DETAIL_VALUE_MAX_LENGTH);
    }
    format_operation_source_prepare(txCtx);
}

static void format_path_payment_strict_receive_receive(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Receive");
    print_amount(txCtx->tx_details.op_details.path_payment_strict_receive_op.dest_amount,
                 &txCtx->tx_details.op_details.path_payment_strict_receive_op.dest_asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    if (txCtx->tx_details.op_details.path_payment_strict_receive_op.path_len) {
        push_to_formatter_stack(&format_path_payment_strict_receive_path_via);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_path_payment_strict_receive_destination(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Destination");
    print_muxed_account(&txCtx->tx_details.op_details.path_payment_strict_receive_op.destination,
                        G_ui_detail_value,
                        DETAIL_VALUE_MAX_LENGTH,
                        0,
                        0);
    push_to_formatter_stack(&format_path_payment_strict_receive_receive);
}

static void format_path_payment_strict_receive_send(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Send Max");
    print_amount(txCtx->tx_details.op_details.path_payment_strict_receive_op.send_max,
                 &txCtx->tx_details.op_details.path_payment_strict_receive_op.send_asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_path_payment_strict_receive_destination);
}

static void format_path_payment_strict_receive(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Path Payment Strict Receive");
    push_to_formatter_stack(&format_path_payment_strict_receive_send);
}

static void format_path_payment_strict_send_path_via(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Via");
    uint8_t i;
    for (i = 0; i < txCtx->tx_details.op_details.path_payment_strict_send_op.path_len; i++) {
        char asset_name[12 + 1];
        asset_t *asset = &txCtx->tx_details.op_details.path_payment_strict_send_op.path[i];
        if (strlen(G_ui_detail_value) != 0) {
            strlcat(G_ui_detail_value, ", ", DETAIL_VALUE_MAX_LENGTH);
        }
        print_asset(asset, txCtx->network, asset_name, sizeof(asset_name));
        strlcat(G_ui_detail_value, asset_name, DETAIL_VALUE_MAX_LENGTH);
    }
    format_operation_source_prepare(txCtx);
}

static void format_path_payment_strict_send_receive(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Receive Min");
    print_amount(txCtx->tx_details.op_details.path_payment_strict_send_op.dest_min,
                 &txCtx->tx_details.op_details.path_payment_strict_send_op.dest_asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    if (txCtx->tx_details.op_details.path_payment_strict_send_op.path_len) {
        push_to_formatter_stack(&format_path_payment_strict_send_path_via);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_path_payment_strict_send_destination(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Destination");
    print_muxed_account(&txCtx->tx_details.op_details.path_payment_strict_send_op.destination,
                        G_ui_detail_value,
                        DETAIL_VALUE_MAX_LENGTH,
                        0,
                        0);
    push_to_formatter_stack(&format_path_payment_strict_send_receive);
}

static void format_path_payment_strict_send(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Send");
    print_amount(txCtx->tx_details.op_details.path_payment_strict_send_op.send_amount,
                 &txCtx->tx_details.op_details.path_payment_strict_send_op.send_asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_path_payment_strict_send_destination);
}

static void format_payment_destination(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Destination");
    print_muxed_account(&txCtx->tx_details.op_details.payment_op.destination,
                        G_ui_detail_value,
                        DETAIL_VALUE_MAX_LENGTH,
                        0,
                        0);
    format_operation_source_prepare(txCtx);
}

static void format_payment_send(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Send");
    print_amount(txCtx->tx_details.op_details.payment_op.amount,
                 &txCtx->tx_details.op_details.payment_op.asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_payment_destination);
}

static void format_payment(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Payment");
    push_to_formatter_stack(&format_payment_send);
}

static void format_create_account_amount(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Starting Balance");
    asset_t asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->tx_details.op_details.create_account_op.starting_balance,
                 &asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_create_account_destination(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Destination");
    print_account_id(txCtx->tx_details.op_details.create_account_op.destination,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    push_to_formatter_stack(&format_create_account_amount);
}

static void format_create_account(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Create Account");
    push_to_formatter_stack(&format_create_account_destination);
}

void format_create_claimable_balance_warning(tx_ctx_t *txCtx) {
    (void) txCtx;
    // TODO: The claimant can be very complicated. I haven't figured out how to
    // display it for the time being, so let's display an WARNING here first.
    strcpy(G_ui_detail_caption, "WARNING");
    strcpy(G_ui_detail_value, "Currently does not support displaying claimant details");
    format_operation_source_prepare(txCtx);
}

static void format_create_claimable_balance_balance(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Balance");
    print_amount(txCtx->tx_details.op_details.create_claimable_balance_op.amount,
                 &txCtx->tx_details.op_details.create_claimable_balance_op.asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_create_claimable_balance_warning);
}

static void format_create_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Create Claimable Balance");
    push_to_formatter_stack(&format_create_claimable_balance_balance);
}

static void format_claim_claimable_balance_balance_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Balance ID");
    print_claimable_balance_id(&txCtx->tx_details.op_details.claim_claimable_balance_op.balance_id,
                               G_ui_detail_value,
                               DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_claim_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Claim Claimable Balance");
    push_to_formatter_stack(&format_claim_claimable_balance_balance_id);
}

static void format_claim_claimable_balance_sponsored_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Sponsored ID");
    print_account_id(txCtx->tx_details.op_details.begin_sponsoring_future_reserves_op.sponsored_id,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    format_operation_source_prepare(txCtx);
}

static void format_begin_sponsoring_future_reserves(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Begin Sponsoring Future Reserves");
    push_to_formatter_stack(&format_claim_claimable_balance_sponsored_id);
}

static void format_end_sponsoring_future_reserves(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "End Sponsoring Future Reserves");
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_account(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Account ID");
    print_account_id(
        txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.account.account_id,
        G_ui_detail_value,
        DETAIL_VALUE_MAX_LENGTH,
        0,
        0);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_trust_line_asset(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.trust_line.asset.type ==
        ASSET_TYPE_POOL_SHARE) {
        strcpy(G_ui_detail_caption, "Liquidity Pool ID");
        print_binary(txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.trust_line.asset
                         .liquidity_pool_id,
                     LIQUIDITY_POOL_ID_SIZE,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    } else {
        strcpy(G_ui_detail_caption, "Asset");
        print_asset((asset_t *) &txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key
                        .trust_line.asset,
                    txCtx->network,
                    G_ui_detail_value,
                    DETAIL_VALUE_MAX_LENGTH);
    }
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_trust_line_account(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Account ID");
    print_account_id(
        txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.trust_line.account_id,
        G_ui_detail_value,
        DETAIL_VALUE_MAX_LENGTH,
        0,
        0);
    push_to_formatter_stack(&format_revoke_sponsorship_trust_line_asset);
}
static void format_revoke_sponsorship_offer_offer_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Offer ID");
    print_uint(txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.offer.offer_id,
               G_ui_detail_value,
               DETAIL_VALUE_MAX_LENGTH);

    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_offer_seller_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Seller ID");
    print_account_id(txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.offer.seller_id,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_offer_offer_id);
}

static void format_revoke_sponsorship_data_data_name(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Data Name");

    _Static_assert(DATA_NAME_MAX_SIZE + 1 < DETAIL_VALUE_MAX_LENGTH,
                   "DATA_NAME_MAX_SIZE must be smaller than DETAIL_VALUE_MAX_LENGTH");

    memcpy(G_ui_detail_value,
           txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.data.data_name,
           txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.data.data_name_size);
    G_ui_detail_value[txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.data
                          .data_name_size] = '\0';
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_data_account(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Account ID");
    print_account_id(txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.data.account_id,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_data_data_name);
}

static void format_revoke_sponsorship_claimable_balance(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Balance ID");
    print_claimable_balance_id(
        &txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.claimable_balance.balance_id,
        G_ui_detail_value,
        DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_liquidity_pool(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Liquidity Pool ID");
    print_binary(txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.liquidity_pool
                     .liquidity_pool_id,
                 LIQUIDITY_POOL_ID_SIZE,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH,
                 0,
                 0);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_claimable_signer_signer_key_detail(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Signer Key");
    signer_key_t *key = &txCtx->tx_details.op_details.revoke_sponsorship_op.signer.signer_key;

    switch (key->type) {
        case SIGNER_KEY_TYPE_ED25519: {
            print_account_id(key->ed25519, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 0, 0);
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            char tmp[57];
            encode_hash_x_key(key->hash_x, tmp, DETAIL_VALUE_MAX_LENGTH);
            print_summary(tmp, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 12, 12);
            break;
        }

        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            char tmp[57];
            encode_pre_auth_x_key(key->pre_auth_tx, tmp, DETAIL_VALUE_MAX_LENGTH);
            print_summary(tmp, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH, 12, 12);
            break;
        }
        default:
            break;
    }
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_claimable_signer_signer_key_type(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Signer Key Type");
    switch (txCtx->tx_details.op_details.revoke_sponsorship_op.signer.signer_key.type) {
        case SIGNER_KEY_TYPE_ED25519: {
            strcpy(G_ui_detail_value, "Public Key");
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            strcpy(G_ui_detail_value, "Hash(x)");
            break;
        }
        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            strcpy(G_ui_detail_value, "Pre-Auth");
            break;
        }
        default:
            break;
    }

    push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_signer_key_detail);
}

static void format_revoke_sponsorship_claimable_signer_account(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Account ID");
    print_account_id(txCtx->tx_details.op_details.revoke_sponsorship_op.signer.account_id,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_signer_key_type);
}

static void format_revoke_sponsorship(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Operation Type");
    if (txCtx->tx_details.op_details.revoke_sponsorship_op.type == REVOKE_SPONSORSHIP_SIGNER) {
        strcpy(G_ui_detail_value, "Revoke Sponsorship (SIGNER_KEY)");
        push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_account);
    } else {
        switch (txCtx->tx_details.op_details.revoke_sponsorship_op.ledger_key.type) {
            case ACCOUNT:
                strcpy(G_ui_detail_value, "Revoke Sponsorship (ACCOUNT)");
                push_to_formatter_stack(&format_revoke_sponsorship_account);
                break;
            case OFFER:
                strcpy(G_ui_detail_value, "Revoke Sponsorship (OFFER)");
                push_to_formatter_stack(&format_revoke_sponsorship_offer_seller_id);
                break;
            case TRUSTLINE:
                strcpy(G_ui_detail_value, "Revoke Sponsorship (TRUSTLINE)");
                push_to_formatter_stack(&format_revoke_sponsorship_trust_line_account);
                break;
            case DATA:
                strcpy(G_ui_detail_value, "Revoke Sponsorship (DATA)");
                push_to_formatter_stack(&format_revoke_sponsorship_data_account);
                break;
            case CLAIMABLE_BALANCE:
                strcpy(G_ui_detail_value, "Revoke Sponsorship (CLAIMABLE_BALANCE)");
                push_to_formatter_stack(&format_revoke_sponsorship_claimable_balance);
                break;
            case LIQUIDITY_POOL:
                strcpy(G_ui_detail_value, "Revoke Sponsorship (LIQUIDITY_POOL)");
                push_to_formatter_stack(&format_revoke_sponsorship_liquidity_pool);
                break;
        }
    }
}

static void format_clawback_from(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "From");
    print_muxed_account(&txCtx->tx_details.op_details.clawback_op.from,
                        G_ui_detail_value,
                        DETAIL_VALUE_MAX_LENGTH,
                        0,
                        0);
    format_operation_source_prepare(txCtx);
}

static void format_clawback_amount(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Clawback Balance");
    print_amount(txCtx->tx_details.op_details.clawback_op.amount,
                 &txCtx->tx_details.op_details.clawback_op.asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_clawback_from);
}

static void format_clawback(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Clawback");
    push_to_formatter_stack(&format_clawback_amount);
}

static void format_clawback_claimable_balance_balance_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Balance ID");
    print_claimable_balance_id(
        &txCtx->tx_details.op_details.clawback_claimable_balance_op.balance_id,
        G_ui_detail_value,
        DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_clawback_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Clawback Claimable Balance");
    push_to_formatter_stack(&format_clawback_claimable_balance_balance_id);
}

static void format_set_trust_line_set_flags(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Set Flags");
    if (txCtx->tx_details.op_details.set_trust_line_flags_op.set_flags) {
        print_trust_line_flags(txCtx->tx_details.op_details.set_trust_line_flags_op.set_flags,
                               G_ui_detail_value,
                               DETAIL_VALUE_MAX_LENGTH);
    } else {
        strcpy(G_ui_detail_value, "[none]");
    }
    format_operation_source_prepare(txCtx);
}

static void format_set_trust_line_clear_flags(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Clear Flags");
    if (txCtx->tx_details.op_details.set_trust_line_flags_op.clear_flags) {
        print_trust_line_flags(txCtx->tx_details.op_details.set_trust_line_flags_op.clear_flags,
                               G_ui_detail_value,
                               DETAIL_VALUE_MAX_LENGTH);
    } else {
        strcpy(G_ui_detail_value, "[none]");
    }
    push_to_formatter_stack(&format_set_trust_line_set_flags);
}

static void format_set_trust_line_asset(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Asset");
    print_asset(&txCtx->tx_details.op_details.set_trust_line_flags_op.asset,
                txCtx->network,
                G_ui_detail_value,
                DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_set_trust_line_clear_flags);
}

static void format_set_trust_line_trustor(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Trustor");
    print_account_id(txCtx->tx_details.op_details.set_trust_line_flags_op.trustor,
                     G_ui_detail_value,
                     DETAIL_VALUE_MAX_LENGTH,
                     0,
                     0);
    push_to_formatter_stack(&format_set_trust_line_asset);
}

static void format_set_trust_line_flags(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Set Trust Line Flags");
    push_to_formatter_stack(&format_set_trust_line_trustor);
}

static void format_liquidity_pool_deposit_max_price(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Max Price");
    uint64_t price =
        ((uint64_t) txCtx->tx_details.op_details.liquidity_pool_deposit_op.max_price.n * 10000000) /
        txCtx->tx_details.op_details.liquidity_pool_deposit_op.max_price.d;
    print_amount(price, NULL, txCtx->network, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_liquidity_pool_deposit_min_price(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Min Price");
    uint64_t price =
        ((uint64_t) txCtx->tx_details.op_details.liquidity_pool_deposit_op.min_price.n * 10000000) /
        txCtx->tx_details.op_details.liquidity_pool_deposit_op.min_price.d;
    print_amount(price, NULL, txCtx->network, G_ui_detail_value, DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_price);
}

static void format_liquidity_pool_deposit_max_amount_b(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Max Amount B");
    print_amount(txCtx->tx_details.op_details.liquidity_pool_deposit_op.max_amount_b,
                 NULL,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_liquidity_pool_deposit_min_price);
}

static void format_liquidity_pool_deposit_max_amount_a(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Max Amount A");
    print_amount(txCtx->tx_details.op_details.liquidity_pool_deposit_op.max_amount_a,
                 NULL,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_amount_b);
}

static void format_liquidity_pool_deposit_liquidity_pool_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Liquidity Pool ID");
    print_binary(txCtx->tx_details.op_details.liquidity_pool_deposit_op.liquidity_pool_id,
                 LIQUIDITY_POOL_ID_SIZE,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH,
                 0,
                 0);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_amount_a);
}

static void format_liquidity_pool_deposit(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Liquidity Pool Deposit");
    push_to_formatter_stack(&format_liquidity_pool_deposit_liquidity_pool_id);
}

static void format_liquidity_pool_withdraw_min_amount_b(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Min Amount B");
    print_amount(txCtx->tx_details.op_details.liquidity_pool_withdraw_op.min_amount_b,
                 NULL,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    format_operation_source_prepare(txCtx);
}

static void format_liquidity_pool_withdraw_min_amount_a(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Min Amount A");
    print_amount(txCtx->tx_details.op_details.liquidity_pool_withdraw_op.min_amount_a,
                 NULL,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_min_amount_b);
}

static void format_liquidity_pool_withdraw_amount(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Amount");
    print_amount(txCtx->tx_details.op_details.liquidity_pool_withdraw_op.amount,
                 NULL,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_min_amount_a);
}

static void format_liquidity_pool_withdraw_liquidity_pool_id(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Liquidity Pool ID");
    print_binary(txCtx->tx_details.op_details.liquidity_pool_withdraw_op.liquidity_pool_id,
                 LIQUIDITY_POOL_ID_SIZE,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH,
                 0,
                 0);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_amount);
}

static void format_liquidity_pool_withdraw(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Operation Type");
    strcpy(G_ui_detail_value, "Liquidity Pool Withdraw");
    push_to_formatter_stack(&format_liquidity_pool_withdraw_liquidity_pool_id);
}

static const format_function_t formatters[] = {&format_create_account,
                                               &format_payment,
                                               &format_path_payment_strict_receive,
                                               &format_manage_sell_offer,
                                               &format_create_passive_sell_offer,
                                               &format_set_options,
                                               &format_change_trust,
                                               &format_allow_trust,
                                               &format_account_merge,
                                               &format_inflation,
                                               &format_manage_data,
                                               &format_bump_sequence,
                                               &format_manage_buy_offer,
                                               &format_path_payment_strict_send,
                                               &format_create_claimable_balance,
                                               &format_claim_claimable_balance,
                                               &format_begin_sponsoring_future_reserves,
                                               &format_end_sponsoring_future_reserves,
                                               &format_revoke_sponsorship,
                                               &format_clawback,
                                               &format_clawback_claimable_balance,
                                               &format_set_trust_line_flags,
                                               &format_liquidity_pool_deposit,
                                               &format_liquidity_pool_withdraw};

void format_confirm_operation(tx_ctx_t *txCtx) {
    if (txCtx->tx_details.operations_len > 1) {
        size_t len;
        strcpy(op_caption, "Operation ");
        len = strlen(op_caption);
        print_uint(txCtx->tx_details.operation_index,
                   op_caption + len,
                   OPERATION_CAPTION_MAX_SIZE - len);
        strlcat(op_caption, " of ", sizeof(op_caption));
        len = strlen(op_caption);
        print_uint(txCtx->tx_details.operations_len,
                   op_caption + len,
                   OPERATION_CAPTION_MAX_SIZE - len);
        push_to_formatter_stack(
            ((format_function_t) PIC(formatters[txCtx->tx_details.op_details.type])));
    } else {
        ((format_function_t) PIC(formatters[txCtx->tx_details.op_details.type]))(txCtx);
    }
}

static void format_fee_bump_transaction_fee(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Max Fee");
    asset_t asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->fee_bump_tx_details.fee,
                 &asset,
                 txCtx->network,
                 G_ui_detail_value,
                 DETAIL_VALUE_MAX_LENGTH);
    push_to_formatter_stack(&format_transaction_details);
}

static void format_fee_bump_transaction_source(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Fee Source");
    if (txCtx->envelope_type == ENVELOPE_TYPE_TX_FEE_BUMP &&
        txCtx->fee_bump_tx_details.fee_source.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->fee_bump_tx_details.fee_source.ed25519,
               G_context.raw_public_key,
               RAW_ED25519_PUBLIC_KEY_SIZE) == 0) {
        print_muxed_account(&txCtx->fee_bump_tx_details.fee_source,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            6,
                            6);
    } else {
        print_muxed_account(&txCtx->fee_bump_tx_details.fee_source,
                            G_ui_detail_value,
                            DETAIL_VALUE_MAX_LENGTH,
                            0,
                            0);
    }
    push_to_formatter_stack(&format_fee_bump_transaction_fee);
}

static void format_fee_bump_transaction_details(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(G_ui_detail_caption, "Fee Bump");
    strcpy(G_ui_detail_value, "Transaction Details");
    push_to_formatter_stack(&format_fee_bump_transaction_source);
}

static format_function_t get_tx_details_formatter(tx_ctx_t *txCtx) {
    if (txCtx->envelope_type == ENVELOPE_TYPE_TX_FEE_BUMP) {
        return &format_fee_bump_transaction_details;
    }
    if (txCtx->envelope_type == ENVELOPE_TYPE_TX) {
        if (txCtx->tx_details.memo.type != MEMO_NONE) {
            return &format_memo;
        } else {
            return &format_fee;
        }
    } else {
        THROW(0x6125);
    }
}

static void format_network(tx_ctx_t *txCtx) {
    strcpy(G_ui_detail_caption, "Network");
    strlcpy(G_ui_detail_value,
            (char *) PIC(NETWORK_NAMES[txCtx->network]),
            DETAIL_VALUE_MAX_LENGTH);
    format_function_t formatter = get_tx_details_formatter(txCtx);
    push_to_formatter_stack(formatter);
}

static format_function_t get_tx_formatter(tx_ctx_t *txCtx) {
    if (txCtx->network != 0) {
        return &format_network;
    } else {
        return get_tx_details_formatter(txCtx);
    }
}

format_function_t get_formatter(tx_ctx_t *txCtx, bool forward) {
    if (!forward) {
        if (G_ui_current_data_index ==
            0) {  // if we're already at the beginning of the buffer, return NULL
            return NULL;
        }
        // rewind to tx beginning if we're requesting a previous operation
        txCtx->offset = 0;
        txCtx->tx_details.operation_index = 0;
    }

    if (G_ui_current_data_index == 1) {
        return get_tx_formatter(txCtx);
    }

    // 1 == data_count_before_ops
    while (G_ui_current_data_index - 1 > txCtx->tx_details.operation_index) {
        if (!parse_tx_xdr(txCtx->raw, txCtx->raw_length, txCtx)) {
            return NULL;
        }
    }
    return &format_confirm_operation;
}

void ui_approve_tx_next_screen(tx_ctx_t *txCtx) {
    if (!formatter_stack[formatter_index]) {
        explicit_bzero(formatter_stack, sizeof(formatter_stack));
        formatter_index = 0;
        G_ui_current_data_index++;
        formatter_stack[0] = get_formatter(txCtx, true);
    }
}

void ui_approve_tx_prev_screen(tx_ctx_t *txCtx) {
    if (formatter_index == -1) {
        explicit_bzero(formatter_stack, sizeof(formatter_stack));
        formatter_index = 0;
        G_ui_current_data_index--;
        formatter_stack[0] = get_formatter(txCtx, false);
    }
}

void set_state_data(bool forward) {
    PRINTF("set_state_data invoked, forward = %d\n", forward);
    if (forward) {
        ui_approve_tx_next_screen(&G_context.tx_info);
    } else {
        ui_approve_tx_prev_screen(&G_context.tx_info);
    }

    // Apply last formatter to fill the screen's buffer
    if (formatter_stack[formatter_index]) {
        explicit_bzero(G_ui_detail_caption, sizeof(G_ui_detail_caption));
        explicit_bzero(G_ui_detail_value, sizeof(G_ui_detail_value));
        explicit_bzero(op_caption, sizeof(op_caption));
        formatter_stack[formatter_index](&G_context.tx_info);

        if (op_caption[0] != '\0') {
            strlcpy(G_ui_detail_caption, op_caption, sizeof(G_ui_detail_caption));
            G_ui_detail_value[0] = ' ';
        }
    }
}