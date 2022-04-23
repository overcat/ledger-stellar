#include <stdbool.h>  // bool
#include <string.h>   // memset
#include "../globals.h"
#include "../common/format.h"
#include "../utils.h"
#include "tx_parser.h"
#include "transaction_formatter.h"

char opCaption[OPERATION_CAPTION_MAX_SIZE];
char detailCaption[DETAIL_CAPTION_MAX_SIZE];
char detailValue[DETAIL_VALUE_MAX_SIZE];

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
    strcpy(detailCaption, "Tx Source");
    // TODO: fix G_context.raw_public_key
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX &&
        txCtx->txDetails.sourceAccount.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->txDetails.sourceAccount.ed25519,
               G_context.raw_public_key,
               ED25519_PUBLIC_KEY_LEN) == 0) {
        print_muxed_account(&txCtx->txDetails.sourceAccount,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            6,
                            6);
    } else {
        print_muxed_account(&txCtx->txDetails.sourceAccount,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            0,
                            0);
    }
    push_to_formatter_stack(format_next_step);
}

static void format_min_seq_ledger_gap(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Seq Ledger Gap");
    print_uint(txCtx->txDetails.cond.minSeqLedgerGap, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_transaction_source);
}

static void format_min_seq_ledger_gap_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.cond.minSeqLedgerGap == 0) {
        format_transaction_source(txCtx);
    } else {
        format_min_seq_ledger_gap(txCtx);
    }
}

static void format_min_seq_age(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Seq Age");
    print_uint(txCtx->txDetails.cond.minSeqAge, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_min_seq_ledger_gap_prepare);
}

static void format_min_seq_age_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.cond.minSeqAge == 0) {
        format_min_seq_ledger_gap_prepare(txCtx);
    } else {
        format_min_seq_age(txCtx);
    }
}

static void format_min_seq_num(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Seq Num");
    print_uint(txCtx->txDetails.cond.minSeqNum, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_min_seq_age_prepare);
}

static void format_min_seq_num_prepare(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.cond.hasMinSeqNum || txCtx->txDetails.cond.minSeqNum == 0) {
        format_min_seq_age_prepare(txCtx);
    } else {
        format_min_seq_num(txCtx);
    }
}

static void format_ledger_bounds_max_ledger(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Ledger Bounds Max");
    print_uint(txCtx->txDetails.cond.ledgerBounds.maxLedger, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_min_seq_num_prepare);
}

static void format_ledger_bounds_min_ledger(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Ledger Bounds Min");
    print_uint(txCtx->txDetails.cond.ledgerBounds.minLedger, detailValue, DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.cond.ledgerBounds.maxLedger != 0) {
        push_to_formatter_stack(&format_ledger_bounds_max_ledger);
    } else {
        push_to_formatter_stack(&format_min_seq_num_prepare);
    }
}

static void format_ledger_bounds(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.cond.hasLedgerBounds ||
        (txCtx->txDetails.cond.ledgerBounds.minLedger == 0 &&
         txCtx->txDetails.cond.ledgerBounds.maxLedger == 0)) {
        format_min_seq_num_prepare(txCtx);
    } else if (txCtx->txDetails.cond.ledgerBounds.minLedger != 0) {
        format_ledger_bounds_min_ledger(txCtx);
    } else {
        format_ledger_bounds_max_ledger(txCtx);
    }
}

static void format_time_bounds_max_time(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Valid Before (UTC)");
    if (!print_time(txCtx->txDetails.cond.timeBounds.maxTime, detailValue, DETAIL_VALUE_MAX_SIZE)) {
        THROW(0x6126);
    };
    push_to_formatter_stack(&format_ledger_bounds);
}

static void format_time_bounds_min_time(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Valid After (UTC)");
    if (!print_time(txCtx->txDetails.cond.timeBounds.minTime, detailValue, DETAIL_VALUE_MAX_SIZE)) {
        THROW(0x6126);
    };

    if (txCtx->txDetails.cond.timeBounds.maxTime != 0) {
        push_to_formatter_stack(&format_time_bounds_max_time);
    } else {
        push_to_formatter_stack(&format_ledger_bounds);
    }
}

static void format_time_bounds(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.cond.hasTimeBounds || (txCtx->txDetails.cond.timeBounds.minTime == 0 &&
                                                 txCtx->txDetails.cond.timeBounds.maxTime == 0)) {
        format_ledger_bounds(txCtx);
    } else if (txCtx->txDetails.cond.timeBounds.minTime != 0) {
        format_time_bounds_min_time(txCtx);
    } else {
        format_time_bounds_max_time(txCtx);
    }
}

static void format_sequence(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sequence Num");
    print_uint(txCtx->txDetails.sequenceNumber, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_time_bounds);
}

static void format_fee(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Fee");
    Asset asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->txDetails.fee, &asset, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_sequence);
}

static void format_memo(tx_ctx_t *txCtx) {
    Memo *memo = &txCtx->txDetails.memo;
    switch (memo->type) {
        case MEMO_ID: {
            strcpy(detailCaption, "Memo ID");
            print_uint(memo->id, detailValue, DETAIL_VALUE_MAX_SIZE);
            break;
        }
        case MEMO_TEXT: {
            strcpy(detailCaption, "Memo Text");
            strlcpy(detailValue, memo->text, MEMO_TEXT_MAX_SIZE + 1);
            break;
        }
        case MEMO_HASH: {
            strcpy(detailCaption, "Memo Hash");
            print_binary(memo->hash, HASH_SIZE, detailValue, DETAIL_VALUE_MAX_SIZE, 6, 6);
            break;
        }
        case MEMO_RETURN: {
            strcpy(detailCaption, "Memo Return");
            print_binary(memo->hash, HASH_SIZE, detailValue, DETAIL_VALUE_MAX_SIZE, 6, 6);
            break;
        }
        default: {
            // TODO: remove the branch
            strcpy(detailCaption, "Memo");
            strcpy(detailValue, "[none]");
        }
    }
    push_to_formatter_stack(&format_fee);
}

static void format_transaction_details(tx_ctx_t *txCtx) {
    switch (txCtx->envelopeType) {
        case ENVELOPE_TYPE_TX_FEE_BUMP:
            strcpy(detailCaption, "InnerTx");
            break;
        case ENVELOPE_TYPE_TX:
            strcpy(detailCaption, "Transaction");
            break;
    }
    strcpy(detailValue, "Details");
    if (txCtx->txDetails.memo.text != MEMO_NONE) {
        push_to_formatter_stack(&format_memo);
    } else {
        push_to_formatter_stack(&format_fee);
    }
}

static void format_operation_source(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Op Source");
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX &&
        txCtx->txDetails.sourceAccount.type == KEY_TYPE_ED25519 &&
        txCtx->txDetails.opDetails.sourceAccount.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->txDetails.sourceAccount.ed25519,
               G_context.raw_public_key,
               ED25519_PUBLIC_KEY_LEN) == 0 &&
        memcmp(txCtx->txDetails.opDetails.sourceAccount.ed25519,
               G_context.raw_public_key,
               ED25519_PUBLIC_KEY_LEN) == 0) {
        print_muxed_account(&txCtx->txDetails.opDetails.sourceAccount,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            6,
                            6);
    } else {
        print_muxed_account(&txCtx->txDetails.opDetails.sourceAccount,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            0,
                            0);
    }

    if (txCtx->txDetails.opIdx == txCtx->txDetails.opCount) {
        // last operation
        push_to_formatter_stack(NULL);
    } else {
        // more operations
        push_to_formatter_stack(&format_next_step);
    }
}

static void format_operation_source_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.sourceAccountPresent) {
        // If the source exists, when the user clicks the next button,
        // it will jump to the page showing the source
        push_to_formatter_stack(&format_operation_source);
    } else {
        // If not, jump to the signing page or show the next operation.
        if (txCtx->txDetails.opIdx == txCtx->txDetails.opCount) {
            // last operation
            push_to_formatter_stack(NULL);
        } else {
            // more operations
            push_to_formatter_stack(&format_next_step);
        }
    }
}

static void format_bump_sequence_bump_to(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Bump To");
    print_int(txCtx->txDetails.opDetails.bumpSequenceOp.bumpTo, detailValue, DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_bump_sequence(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Bump Sequence");
    push_to_formatter_stack(&format_bump_sequence_bump_to);
}

static void format_inflation(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Inflation");
    format_operation_source_prepare(txCtx);
}

static void format_account_merge_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.destination,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE,
                        0,
                        0);
    format_operation_source_prepare(txCtx);
}

static void format_account_merge_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Merge Account");
    if (txCtx->txDetails.opDetails.sourceAccountPresent) {
        print_muxed_account(&txCtx->txDetails.opDetails.sourceAccount,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            0,
                            0);
    } else {
        print_muxed_account(&txCtx->txDetails.sourceAccount,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            0,
                            0);
    }
    push_to_formatter_stack(&format_account_merge_destination);
}

static void format_account_merge(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Account Merge");
    push_to_formatter_stack(&format_account_merge_detail);
}

static void format_manage_data_value(tx_ctx_t *txCtx) {
    // TODO: if printable?
    strcpy(detailCaption, "Data Value");
    char tmp[89];
    base64_encode(txCtx->txDetails.opDetails.manageDataOp.dataValue,
                  txCtx->txDetails.opDetails.manageDataOp.dataValueSize,
                  tmp,
                  sizeof(tmp));
    print_summary(tmp, detailValue, DETAIL_VALUE_MAX_SIZE, 12, 12);
    format_operation_source_prepare(txCtx);
}

static void format_manage_data_detail(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.manageDataOp.dataValueSize) {
        strcpy(detailCaption, "Set Data");
        push_to_formatter_stack(&format_manage_data_value);
    } else {
        strcpy(detailCaption, "Remove Data");
        format_operation_source_prepare(txCtx);
    }
    char tmp[65];
    memcpy(tmp,
           txCtx->txDetails.opDetails.manageDataOp.dataName,
           txCtx->txDetails.opDetails.manageDataOp.dataNameSize);
    tmp[txCtx->txDetails.opDetails.manageDataOp.dataNameSize] = '\0';
    print_summary(tmp, detailValue, DETAIL_VALUE_MAX_SIZE, 12, 12);
}

static void format_manage_data(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Manage Data");
    push_to_formatter_stack(&format_manage_data_detail);
}

static void format_allow_trust_authorize(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Authorize Flag");
    if (txCtx->txDetails.opDetails.allowTrustOp.authorize == AUTHORIZED_FLAG) {
        strlcpy(detailValue, "AUTHORIZED_FLAG", DETAIL_VALUE_MAX_SIZE);
    } else if (txCtx->txDetails.opDetails.allowTrustOp.authorize ==
               AUTHORIZED_TO_MAINTAIN_LIABILITIES_FLAG) {
        strlcpy(detailValue, "AUTHORIZED_TO_MAINTAIN_LIABILITIES_FLAG", DETAIL_VALUE_MAX_SIZE);
    } else {
        strlcpy(detailValue, "UNAUTHORIZED_FLAG", DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_allow_trust_asset_code(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset Code");
    strlcpy(detailValue, txCtx->txDetails.opDetails.allowTrustOp.assetCode, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_allow_trust_authorize);
}

static void format_allow_trust_trustor(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Trustor");
    print_account_id(txCtx->txDetails.opDetails.allowTrustOp.trustor,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_allow_trust_asset_code);
}

static void format_allow_trust(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Allow Trust");
    push_to_formatter_stack(&format_allow_trust_trustor);
}

static void format_set_option_signer_weight(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Weight");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.signer.weight,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_set_option_signer_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Signer Key");
    SignerKey *key = &txCtx->txDetails.opDetails.setOptionsOp.signer.key;

    switch (key->type) {
        case SIGNER_KEY_TYPE_ED25519: {
            print_account_id(key->data, detailValue, DETAIL_VALUE_MAX_SIZE, 0, 0);
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            char tmp[57];
            encode_hash_x_key(key->data, tmp, DETAIL_VALUE_MAX_SIZE);
            print_summary(tmp, detailValue, DETAIL_VALUE_MAX_SIZE, 12, 12);
            break;
        }

        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            char tmp[57];
            encode_pre_auth_x_key(key->data, tmp, DETAIL_VALUE_MAX_SIZE);
            print_summary(tmp, detailValue, DETAIL_VALUE_MAX_SIZE, 12, 12);
            break;
        }
        default:
            break;
    }
    push_to_formatter_stack(&format_set_option_signer_weight);
}

static void format_set_option_signer(tx_ctx_t *txCtx) {
    signer_t *signer = &txCtx->txDetails.opDetails.setOptionsOp.signer;
    if (signer->weight) {
        strcpy(detailCaption, "Add Signer");
    } else {
        strcpy(detailCaption, "Remove Signer");
    }
    switch (signer->key.type) {
        case SIGNER_KEY_TYPE_ED25519: {
            strcpy(detailValue, "Type Public Key");
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            strcpy(detailValue, "Type Hash(x)");
            break;
        }
        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            strcpy(detailValue, "Type Pre-Auth");
            break;
        }
        default:
            break;
    }
    push_to_formatter_stack(&format_set_option_signer_detail);
}

static void format_set_option_signer_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.signerPresent) {
        push_to_formatter_stack(&format_set_option_signer);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_set_option_home_domain(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Home Domain");
    if (txCtx->txDetails.opDetails.setOptionsOp.homeDomainSize) {
        memcpy(detailValue,
               txCtx->txDetails.opDetails.setOptionsOp.homeDomain,
               txCtx->txDetails.opDetails.setOptionsOp.homeDomainSize);
        detailValue[txCtx->txDetails.opDetails.setOptionsOp.homeDomainSize] = '\0';
    } else {
        strcpy(detailValue, "[remove home domain from account]");
    }
    format_set_option_signer_prepare(txCtx);
}

static void format_set_option_home_domain_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.homeDomainPresent) {
        push_to_formatter_stack(&format_set_option_home_domain);
    } else {
        format_set_option_signer_prepare(txCtx);
    }
}

static void format_set_option_high_threshold(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "High Threshold");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.highThreshold,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_home_domain_prepare(txCtx);
}

static void format_set_option_high_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.highThresholdPresent) {
        push_to_formatter_stack(&format_set_option_high_threshold);
    } else {
        format_set_option_home_domain_prepare(txCtx);
    }
}

static void format_set_option_medium_threshold(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Medium Threshold");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.mediumThreshold,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_high_threshold_prepare(txCtx);
}

static void format_set_option_medium_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.mediumThresholdPresent) {
        push_to_formatter_stack(&format_set_option_medium_threshold);
    } else {
        format_set_option_high_threshold_prepare(txCtx);
    }
}

static void format_set_option_low_threshold(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Low Threshold");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.lowThreshold,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_medium_threshold_prepare(txCtx);
}

static void format_set_option_low_threshold_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.lowThresholdPresent) {
        push_to_formatter_stack(&format_set_option_low_threshold);
    } else {
        format_set_option_medium_threshold_prepare(txCtx);
    }
}

static void format_set_option_master_weight(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Master Weight");
    print_uint(txCtx->txDetails.opDetails.setOptionsOp.masterWeight,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);
    format_set_option_low_threshold_prepare(txCtx);
}

static void format_set_option_master_weight_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.masterWeightPresent) {
        push_to_formatter_stack(&format_set_option_master_weight);
    } else {
        format_set_option_low_threshold_prepare(txCtx);
    }
}

static void format_set_option_set_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Set Flags");
    print_flags(txCtx->txDetails.opDetails.setOptionsOp.setFlags,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    format_set_option_master_weight_prepare(txCtx);
}

static void format_set_option_set_flags_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.setFlags) {
        push_to_formatter_stack(&format_set_option_set_flags);
    } else {
        format_set_option_master_weight_prepare(txCtx);
    }
}

static void format_set_option_clear_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Clear Flags");
    print_flags(txCtx->txDetails.opDetails.setOptionsOp.clearFlags,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    format_set_option_set_flags_prepare(txCtx);
}

static void format_set_option_clear_flags_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.clearFlags) {
        push_to_formatter_stack(&format_set_option_clear_flags);
    } else {
        format_set_option_set_flags_prepare(txCtx);
    }
}

static void format_set_option_inflation_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Inflation Dest");
    print_account_id(txCtx->txDetails.opDetails.setOptionsOp.inflationDestination,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    format_set_option_clear_flags_prepare(txCtx);
}

static void format_set_option_inflation_destination_prepare(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.setOptionsOp.inflationDestinationPresent) {
        push_to_formatter_stack(format_set_option_inflation_destination);
    } else {
        format_set_option_clear_flags_prepare(txCtx);
    }
}

static void format_set_options_empty_body(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "SET OPTIONS");
    strcpy(detailValue, "BODY IS EMPTY");
    format_operation_source_prepare(txCtx);
}

static bool is_empty_set_options_body(tx_ctx_t *txCtx) {
    return !(txCtx->txDetails.opDetails.setOptionsOp.inflationDestinationPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.clearFlags ||
             txCtx->txDetails.opDetails.setOptionsOp.setFlags ||
             txCtx->txDetails.opDetails.setOptionsOp.masterWeightPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.lowThresholdPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.mediumThresholdPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.highThresholdPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.homeDomainPresent ||
             txCtx->txDetails.opDetails.setOptionsOp.signerPresent);
}

static void format_set_options(tx_ctx_t *txCtx) {
    // this operation is a special one among all operations, because all its fields are optional.
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Set Options");
    if (is_empty_set_options_body(txCtx)) {
        push_to_formatter_stack(format_set_options_empty_body);
    } else {
        format_set_option_inflation_destination_prepare(txCtx);
    }
}

static void format_change_trust_limit(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Trust Limit");
    print_amount(txCtx->txDetails.opDetails.changeTrustOp.limit,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_change_trust_detail_liquidity_pool_fee(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Pool Fee Rate");
    uint64_t fee =
        ((uint64_t)
             txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.fee *
         10000000) /
        100;
    print_amount(fee, NULL, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    strlcat(detailValue, "%", DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.opDetails.changeTrustOp.limit &&
        txCtx->txDetails.opDetails.changeTrustOp.limit != INT64_MAX) {
        push_to_formatter_stack(&format_change_trust_limit);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_change_trust_detail_liquidity_pool_asset_b(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset B");
    print_asset(&txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.assetB,
                txCtx->network,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_fee);
}

static void format_change_trust_detail_liquidity_pool_asset_a(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset A");
    print_asset(&txCtx->txDetails.opDetails.changeTrustOp.line.liquidityPool.constantProduct.assetA,
                txCtx->network,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset_b);
}

static void format_change_trust_detail_liquidity_pool_asset(tx_ctx_t *txCtx) {
    (void) txCtx;

    strcpy(detailCaption, "Liquidity Pool");
    strcpy(detailValue, "Asset");
    push_to_formatter_stack(&format_change_trust_detail_liquidity_pool_asset_a);
}

static void format_change_trust_detail(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.changeTrustOp.limit) {
        strcpy(detailCaption, "Change Trust");
    } else {
        strcpy(detailCaption, "Remove Trust");
    }
    uint8_t asset_type = txCtx->txDetails.opDetails.changeTrustOp.line.type;
    switch (asset_type) {
        case ASSET_TYPE_CREDIT_ALPHANUM4:
        case ASSET_TYPE_CREDIT_ALPHANUM12:
            print_asset((Asset *) &txCtx->txDetails.opDetails.changeTrustOp.line,
                        txCtx->network,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE);
            if (txCtx->txDetails.opDetails.changeTrustOp.limit &&
                txCtx->txDetails.opDetails.changeTrustOp.limit != INT64_MAX) {
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
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Change Trust");
    push_to_formatter_stack(&format_change_trust_detail);
}

static void format_manage_sell_offer_sell(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sell");
    print_amount(txCtx->txDetails.opDetails.manageSellOfferOp.amount,
                 &txCtx->txDetails.opDetails.manageSellOfferOp.selling,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_manage_sell_offer_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Price");
    uint64_t price = ((uint64_t) txCtx->txDetails.opDetails.manageSellOfferOp.price.n * 10000000) /
                     txCtx->txDetails.opDetails.manageSellOfferOp.price.d;
    print_amount(price,
                 &txCtx->txDetails.opDetails.manageSellOfferOp.buying,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_manage_sell_offer_sell);
}

static void format_manage_sell_offer_buy(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Buy");
    print_asset(&txCtx->txDetails.opDetails.manageSellOfferOp.buying,
                txCtx->network,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_manage_sell_offer_price);
}

static void format_manage_sell_offer_detail(tx_ctx_t *txCtx) {
    if (!txCtx->txDetails.opDetails.manageSellOfferOp.amount) {
        strcpy(detailCaption, "Remove Offer");
        print_uint(txCtx->txDetails.opDetails.manageSellOfferOp.offerID,
                   detailValue,
                   DETAIL_VALUE_MAX_SIZE);
        format_operation_source_prepare(txCtx);
    } else {
        if (txCtx->txDetails.opDetails.manageSellOfferOp.offerID) {
            strcpy(detailCaption, "Change Offer");
            print_uint(txCtx->txDetails.opDetails.manageSellOfferOp.offerID,
                       detailValue,
                       DETAIL_VALUE_MAX_SIZE);
        } else {
            strcpy(detailCaption, "Create Offer");
            strcpy(detailValue, "Type Active");
        }
        push_to_formatter_stack(&format_manage_sell_offer_buy);
    }
}

static void format_manage_sell_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Manage Sell Offer");
    push_to_formatter_stack(&format_manage_sell_offer_detail);
}

static void format_manage_buy_offer_buy(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    strcpy(detailCaption, "Buy");
    print_amount(op->buyAmount, &op->buying, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_manage_buy_offer_price(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    strcpy(detailCaption, "Price");
    uint64_t price = ((uint64_t) op->price.n * 10000000) / op->price.d;
    print_amount(price, &op->selling, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_manage_buy_offer_buy);
}

static void format_manage_buy_offer_sell(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    strcpy(detailCaption, "Sell");
    print_asset(&op->selling, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_manage_buy_offer_price);
}

static void format_manage_buy_offer_detail(tx_ctx_t *txCtx) {
    ManageBuyOfferOp *op = &txCtx->txDetails.opDetails.manageBuyOfferOp;

    if (op->buyAmount == 0) {
        strcpy(detailCaption, "Remove Offer");
        print_uint(op->offerID, detailValue, DETAIL_VALUE_MAX_SIZE);
        format_operation_source_prepare(txCtx);  // TODO
    } else {
        if (op->offerID) {
            strcpy(detailCaption, "Change Offer");
            print_uint(op->offerID, detailValue, DETAIL_VALUE_MAX_SIZE);
        } else {
            strcpy(detailCaption, "Create Offer");
            strcpy(detailValue, "Type Active");
        }
        push_to_formatter_stack(&format_manage_buy_offer_sell);
    }
}

static void format_manage_buy_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Manage Buy Offer");
    push_to_formatter_stack(&format_manage_buy_offer_detail);
}

static void format_create_passive_sell_offer_sell(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sell");
    print_amount(txCtx->txDetails.opDetails.createPassiveSellOfferOp.amount,
                 &txCtx->txDetails.opDetails.createPassiveSellOfferOp.selling,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_create_passive_sell_offer_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Price");

    CreatePassiveSellOfferOp *op = &txCtx->txDetails.opDetails.createPassiveSellOfferOp;
    uint64_t price = ((uint64_t) op->price.n * 10000000) / op->price.d;
    print_amount(price, &op->buying, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_create_passive_sell_offer_sell);
}

static void format_create_passive_sell_offer_buy(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Buy");
    print_asset(&txCtx->txDetails.opDetails.createPassiveSellOfferOp.buying,
                txCtx->network,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_create_passive_sell_offer_price);
}

static void format_create_passive_sell_offer(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Create Passive Sell Offer");
    push_to_formatter_stack(&format_create_passive_sell_offer_buy);
}

static void format_path_payment_strict_receive_path_via(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Via");
    uint8_t i;
    for (i = 0; i < txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.pathLen; i++) {
        char asset_name[12 + 1];
        Asset *asset = &txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.path[i];
        if (strlen(detailValue) != 0) {
            strlcat(detailValue, ", ", DETAIL_VALUE_MAX_SIZE);
        }
        print_asset(asset, txCtx->network, asset_name, sizeof(asset_name));
        strlcat(detailValue, asset_name, DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_path_payment_strict_receive_receive(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Receive");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.destAmount,
                 &txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.destAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.pathLen) {
        push_to_formatter_stack(&format_path_payment_strict_receive_path_via);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_path_payment_strict_receive_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.destination,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE,
                        0,
                        0);
    push_to_formatter_stack(&format_path_payment_strict_receive_receive);
}

static void format_path_payment_strict_receive_send(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Send Max");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.sendMax,
                 &txCtx->txDetails.opDetails.pathPaymentStrictReceiveOp.sendAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_path_payment_strict_receive_destination);
}

static void format_path_payment_strict_receive(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Path Payment Strict Receive");
    push_to_formatter_stack(&format_path_payment_strict_receive_send);
}

static void format_path_payment_strict_send_path_via(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Via");
    uint8_t i;
    for (i = 0; i < txCtx->txDetails.opDetails.pathPaymentStrictSendOp.pathLen; i++) {
        char asset_name[12 + 1];
        Asset *asset = &txCtx->txDetails.opDetails.pathPaymentStrictSendOp.path[i];
        if (strlen(detailValue) != 0) {
            strlcat(detailValue, ", ", DETAIL_VALUE_MAX_SIZE);
        }
        print_asset(asset, txCtx->network, asset_name, sizeof(asset_name));
        strlcat(detailValue, asset_name, DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_path_payment_strict_send_receive(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Receive Min");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictSendOp.destMin,
                 &txCtx->txDetails.opDetails.pathPaymentStrictSendOp.destAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    if (txCtx->txDetails.opDetails.pathPaymentStrictSendOp.pathLen) {
        push_to_formatter_stack(&format_path_payment_strict_send_path_via);
    } else {
        format_operation_source_prepare(txCtx);
    }
}

static void format_path_payment_strict_send_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.pathPaymentStrictSendOp.destination,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE,
                        0,
                        0);
    push_to_formatter_stack(&format_path_payment_strict_send_receive);
}

static void format_path_payment_strict_send(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Send");
    print_amount(txCtx->txDetails.opDetails.pathPaymentStrictSendOp.sendAmount,
                 &txCtx->txDetails.opDetails.pathPaymentStrictSendOp.sendAsset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_path_payment_strict_send_destination);
}

static void format_payment_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_muxed_account(&txCtx->txDetails.opDetails.payment.destination,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE,
                        0,
                        0);
    format_operation_source_prepare(txCtx);
}

static void format_payment_send(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Send");
    print_amount(txCtx->txDetails.opDetails.payment.amount,
                 &txCtx->txDetails.opDetails.payment.asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_payment_destination);
}

static void format_payment(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Payment");
    push_to_formatter_stack(&format_payment_send);
}

static void format_create_account_amount(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Starting Balance");
    Asset asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->txDetails.opDetails.createAccount.startingBalance,
                 &asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_create_account_destination(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Destination");
    print_account_id(txCtx->txDetails.opDetails.createAccount.destination,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_create_account_amount);
}

static void format_create_account(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Create Account");
    push_to_formatter_stack(&format_create_account_destination);
}

void format_create_claimable_balance_warning(tx_ctx_t *txCtx) {
    (void) txCtx;
    // TODO: The claimant can be very complicated. I haven't figured out how to
    // display it for the time being, so let's display an WARNING here first.
    strcpy(detailCaption, "WARNING");
    strcpy(detailValue, "Currently does not support displaying claimant details");
    format_operation_source_prepare(txCtx);
}

static void format_create_claimable_balance_balance(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance");
    print_amount(txCtx->txDetails.opDetails.createClaimableBalanceOp.amount,
                 &txCtx->txDetails.opDetails.createClaimableBalanceOp.asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_create_claimable_balance_warning);
}

static void format_create_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Create Claimable Balance");
    push_to_formatter_stack(&format_create_claimable_balance_balance);
}

static void format_claim_claimable_balance_balance_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance ID");
    print_claimable_balance_id(&txCtx->txDetails.opDetails.claimClaimableBalanceOp.balanceID,
                               detailValue,
                               DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_claim_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Claim Claimable Balance");
    push_to_formatter_stack(&format_claim_claimable_balance_balance_id);
}

static void format_claim_claimable_balance_sponsored_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Sponsored ID");
    print_account_id(txCtx->txDetails.opDetails.beginSponsoringFutureReservesOp.sponsoredID,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    format_operation_source_prepare(txCtx);
}

static void format_begin_sponsoring_future_reserves(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Begin Sponsoring Future Reserves");
    push_to_formatter_stack(&format_claim_claimable_balance_sponsored_id);
}

static void format_end_sponsoring_future_reserves(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "End Sponsoring Future Reserves");
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_account_id(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.account.accountID,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_trust_line_asset(tx_ctx_t *txCtx) {
    if (txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.asset.type ==
        ASSET_TYPE_POOL_SHARE) {
        strcpy(detailCaption, "Liquidity Pool ID");
        print_binary(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.asset
                         .liquidityPoolID,
                     LIQUIDITY_POOL_ID_SIZE,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    } else {
        strcpy(detailCaption, "Asset");
        print_asset(
            (Asset *) &txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.asset,
            txCtx->network,
            detailValue,
            DETAIL_VALUE_MAX_SIZE);
    }
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_trust_line_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_account_id(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.trustLine.accountID,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_trust_line_asset);
}
static void format_revoke_sponsorship_offer_offer_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Offer ID");
    print_uint(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.offer.offerID,
               detailValue,
               DETAIL_VALUE_MAX_SIZE);

    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_offer_seller_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Seller ID");
    print_account_id(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.offer.sellerID,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_offer_offer_id);
}

static void format_revoke_sponsorship_data_data_name(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Data Name");

    _Static_assert(DATA_NAME_MAX_SIZE + 1 < DETAIL_VALUE_MAX_SIZE,
                   "DATA_NAME_MAX_SIZE must be smaller than DETAIL_VALUE_MAX_SIZE");

    memcpy(detailValue,
           txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.dataName,
           txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.dataNameSize);
    detailValue[txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.dataNameSize] = '\0';
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_data_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_account_id(txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.data.accountID,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_data_data_name);
}

static void format_revoke_sponsorship_claimable_balance(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance ID");
    print_claimable_balance_id(
        &txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.claimableBalance.balanceId,
        detailValue,
        DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_liquidity_pool(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Liquidity Pool ID");
    print_binary(
        txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.liquidityPool.liquidityPoolID,
        LIQUIDITY_POOL_ID_SIZE,
        detailValue,
        DETAIL_VALUE_MAX_SIZE,
        0,
        0);
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_claimable_signer_signer_key_detail(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Signer Key");
    SignerKey *key = &txCtx->txDetails.opDetails.revokeSponsorshipOp.signer.signerKey;

    switch (key->type) {
        case SIGNER_KEY_TYPE_ED25519: {
            print_account_id(key->data, detailValue, DETAIL_VALUE_MAX_SIZE, 0, 0);
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            char tmp[57];
            encode_hash_x_key(key->data, tmp, DETAIL_VALUE_MAX_SIZE);
            print_summary(tmp, detailValue, DETAIL_VALUE_MAX_SIZE, 12, 12);
            break;
        }

        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            char tmp[57];
            encode_pre_auth_x_key(key->data, tmp, DETAIL_VALUE_MAX_SIZE);
            print_summary(tmp, detailValue, DETAIL_VALUE_MAX_SIZE, 12, 12);
            break;
        }
        default:
            break;
    }
    format_operation_source_prepare(txCtx);
}

static void format_revoke_sponsorship_claimable_signer_signer_key_type(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Signer Key Type");
    switch (txCtx->txDetails.opDetails.revokeSponsorshipOp.signer.signerKey.type) {
        case SIGNER_KEY_TYPE_ED25519: {
            strcpy(detailValue, "Public Key");
            break;
        }
        case SIGNER_KEY_TYPE_HASH_X: {
            strcpy(detailValue, "Hash(x)");
            break;
        }
        case SIGNER_KEY_TYPE_PRE_AUTH_TX: {
            strcpy(detailValue, "Pre-Auth");
            break;
        }
        default:
            break;
    }

    push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_signer_key_detail);
}

static void format_revoke_sponsorship_claimable_signer_account(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Account ID");
    print_account_id(txCtx->txDetails.opDetails.revokeSponsorshipOp.signer.accountID,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_signer_key_type);
}

static void format_revoke_sponsorship(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Operation Type");
    if (txCtx->txDetails.opDetails.revokeSponsorshipOp.type == REVOKE_SPONSORSHIP_SIGNER) {
        strcpy(detailValue, "Revoke Sponsorship (SIGNER_KEY)");
        push_to_formatter_stack(&format_revoke_sponsorship_claimable_signer_account);
    } else {
        switch (txCtx->txDetails.opDetails.revokeSponsorshipOp.ledgerKey.type) {
            case ACCOUNT:
                strcpy(detailValue, "Revoke Sponsorship (ACCOUNT)");
                push_to_formatter_stack(&format_revoke_sponsorship_account);
                break;
            case OFFER:
                strcpy(detailValue, "Revoke Sponsorship (OFFER)");
                push_to_formatter_stack(&format_revoke_sponsorship_offer_seller_id);
                break;
            case TRUSTLINE:
                strcpy(detailValue, "Revoke Sponsorship (TRUSTLINE)");
                push_to_formatter_stack(&format_revoke_sponsorship_trust_line_account);
                break;
            case DATA:
                strcpy(detailValue, "Revoke Sponsorship (DATA)");
                push_to_formatter_stack(&format_revoke_sponsorship_data_account);
                break;
            case CLAIMABLE_BALANCE:
                strcpy(detailValue, "Revoke Sponsorship (CLAIMABLE_BALANCE)");
                push_to_formatter_stack(&format_revoke_sponsorship_claimable_balance);
                break;
            case LIQUIDITY_POOL:
                strcpy(detailValue, "Revoke Sponsorship (LIQUIDITY_POOL)");
                push_to_formatter_stack(&format_revoke_sponsorship_liquidity_pool);
                break;
        }
    }
}

static void format_clawback_from(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "From");
    print_muxed_account(&txCtx->txDetails.opDetails.clawbackOp.from,
                        detailValue,
                        DETAIL_VALUE_MAX_SIZE,
                        0,
                        0);
    format_operation_source_prepare(txCtx);
}

static void format_clawback_amount(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Clawback Balance");
    print_amount(txCtx->txDetails.opDetails.clawbackOp.amount,
                 &txCtx->txDetails.opDetails.clawbackOp.asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_clawback_from);
}

static void format_clawback(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Clawback");
    push_to_formatter_stack(&format_clawback_amount);
}

static void format_clawback_claimable_balance_balance_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Balance ID");
    print_claimable_balance_id(&txCtx->txDetails.opDetails.clawbackClaimableBalanceOp.balanceID,
                               detailValue,
                               DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_clawback_claimable_balance(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Clawback Claimable Balance");
    push_to_formatter_stack(&format_clawback_claimable_balance_balance_id);
}

static void format_set_trust_line_set_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Set Flags");
    if (txCtx->txDetails.opDetails.setTrustLineFlagsOp.setFlags) {
        print_trust_line_flags(txCtx->txDetails.opDetails.setTrustLineFlagsOp.setFlags,
                               detailValue,
                               DETAIL_VALUE_MAX_SIZE);
    } else {
        strcpy(detailValue, "[none]");
    }
    format_operation_source_prepare(txCtx);
}

static void format_set_trust_line_clear_flags(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Clear Flags");
    if (txCtx->txDetails.opDetails.setTrustLineFlagsOp.clearFlags) {
        print_trust_line_flags(txCtx->txDetails.opDetails.setTrustLineFlagsOp.clearFlags,
                               detailValue,
                               DETAIL_VALUE_MAX_SIZE);
    } else {
        strcpy(detailValue, "[none]");
    }
    push_to_formatter_stack(&format_set_trust_line_set_flags);
}

static void format_set_trust_line_asset(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Asset");
    print_asset(&txCtx->txDetails.opDetails.setTrustLineFlagsOp.asset,
                txCtx->network,
                detailValue,
                DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_set_trust_line_clear_flags);
}

static void format_set_trust_line_trustor(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Trustor");
    print_account_id(txCtx->txDetails.opDetails.setTrustLineFlagsOp.trustor,
                     detailValue,
                     DETAIL_VALUE_MAX_SIZE,
                     0,
                     0);
    push_to_formatter_stack(&format_set_trust_line_asset);
}

static void format_set_trust_line_flags(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Set Trust Line Flags");
    push_to_formatter_stack(&format_set_trust_line_trustor);
}

static void format_liquidity_pool_deposit_max_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Price");
    uint64_t price =
        ((uint64_t) txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxPrice.n * 10000000) /
        txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxPrice.d;
    print_amount(price, NULL, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_liquidity_pool_deposit_min_price(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Price");
    uint64_t price =
        ((uint64_t) txCtx->txDetails.opDetails.liquidityPoolDepositOp.minPrice.n * 10000000) /
        txCtx->txDetails.opDetails.liquidityPoolDepositOp.minPrice.d;
    print_amount(price, NULL, txCtx->network, detailValue, DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_price);
}

static void format_liquidity_pool_deposit_max_amount_b(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Amount B");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxAmountB,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_min_price);
}

static void format_liquidity_pool_deposit_max_amount_a(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Amount A");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolDepositOp.maxAmountA,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_amount_b);
}

static void format_liquidity_pool_deposit_liquidity_pool_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Liquidity Pool ID");
    print_binary(txCtx->txDetails.opDetails.liquidityPoolDepositOp.liquidityPoolID,
                 LIQUIDITY_POOL_ID_SIZE,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE,
                 0,
                 0);
    push_to_formatter_stack(&format_liquidity_pool_deposit_max_amount_a);
}

static void format_liquidity_pool_deposit(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Liquidity Pool Deposit");
    push_to_formatter_stack(&format_liquidity_pool_deposit_liquidity_pool_id);
}

static void format_liquidity_pool_withdraw_min_amount_b(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Amount B");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.minAmountB,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    format_operation_source_prepare(txCtx);
}

static void format_liquidity_pool_withdraw_min_amount_a(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Min Amount A");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.minAmountA,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_min_amount_b);
}

static void format_liquidity_pool_withdraw_amount(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Amount");
    print_amount(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.amount,
                 NULL,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_min_amount_a);
}

static void format_liquidity_pool_withdraw_liquidity_pool_id(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Liquidity Pool ID");
    print_binary(txCtx->txDetails.opDetails.liquidityPoolWithdrawOp.liquidityPoolID,
                 LIQUIDITY_POOL_ID_SIZE,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE,
                 0,
                 0);
    push_to_formatter_stack(&format_liquidity_pool_withdraw_amount);
}

static void format_liquidity_pool_withdraw(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Operation Type");
    strcpy(detailValue, "Liquidity Pool Withdraw");
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
    if (txCtx->txDetails.opCount > 1) {
        size_t len;
        strcpy(opCaption, "Operation ");
        len = strlen(opCaption);
        print_uint(txCtx->txDetails.opIdx, opCaption + len, OPERATION_CAPTION_MAX_SIZE - len);
        strlcat(opCaption, " of ", sizeof(opCaption));
        len = strlen(opCaption);
        print_uint(txCtx->txDetails.opCount, opCaption + len, OPERATION_CAPTION_MAX_SIZE - len);
        push_to_formatter_stack(
            ((format_function_t) PIC(formatters[txCtx->txDetails.opDetails.type])));
    } else {
        ((format_function_t) PIC(formatters[txCtx->txDetails.opDetails.type]))(txCtx);
    }
}

static void format_fee_bump_transaction_fee(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Max Fee");
    Asset asset = {.type = ASSET_TYPE_NATIVE};
    print_amount(txCtx->feeBumpTxDetails.fee,
                 &asset,
                 txCtx->network,
                 detailValue,
                 DETAIL_VALUE_MAX_SIZE);
    push_to_formatter_stack(&format_transaction_details);
}

static void format_fee_bump_transaction_source(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Fee Source");
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX_FEE_BUMP &&
        txCtx->feeBumpTxDetails.feeSource.type == KEY_TYPE_ED25519 &&
        memcmp(txCtx->feeBumpTxDetails.feeSource.ed25519,
               G_context.raw_public_key,
               ED25519_PUBLIC_KEY_LEN) == 0) {
        print_muxed_account(&txCtx->feeBumpTxDetails.feeSource,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            6,
                            6);
    } else {
        print_muxed_account(&txCtx->feeBumpTxDetails.feeSource,
                            detailValue,
                            DETAIL_VALUE_MAX_SIZE,
                            0,
                            0);
    }
    push_to_formatter_stack(&format_fee_bump_transaction_fee);
}

static void format_fee_bump_transaction_details(tx_ctx_t *txCtx) {
    (void) txCtx;
    strcpy(detailCaption, "Fee Bump");
    strcpy(detailValue, "Transaction Details");
    push_to_formatter_stack(&format_fee_bump_transaction_source);
}

static format_function_t get_tx_details_formatter(tx_ctx_t *txCtx) {
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX_FEE_BUMP) {
        return &format_fee_bump_transaction_details;
    }
    if (txCtx->envelopeType == ENVELOPE_TYPE_TX) {
        if (txCtx->txDetails.memo.text != MEMO_NONE) {
            return &format_memo;
        } else {
            return &format_fee;
        }
    } else {
        THROW(0x6125);
    }
}

static void format_network(tx_ctx_t *txCtx) {
    strcpy(detailCaption, "Network");
    strlcpy(detailValue, (char *) PIC(NETWORK_NAMES[txCtx->network]), DETAIL_VALUE_MAX_SIZE);
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

uint8_t current_data_index;

format_function_t get_formatter(tx_ctx_t *txCtx, bool forward) {
    if (!forward) {
        if (current_data_index ==
            0) {  // if we're already at the beginning of the buffer, return NULL
            return NULL;
        }
        // rewind to tx beginning if we're requesting a previous operation
        txCtx->offset = 0;
        txCtx->txDetails.opIdx = 0;
    }

    if (current_data_index == 1) {
        return get_tx_formatter(txCtx);
    }

    // 1 == data_count_before_ops
    while (current_data_index - 1 > txCtx->txDetails.opIdx) {
        if (!parse_tx_xdr(txCtx->raw, txCtx->rawLength, txCtx)) {
            return NULL;
        }
    }
    return &format_confirm_operation;
}

void ui_approve_tx_next_screen(tx_ctx_t *txCtx) {
    if (!formatter_stack[formatter_index]) {
        MEMCLEAR(formatter_stack);
        formatter_index = 0;
        current_data_index++;
        formatter_stack[0] = get_formatter(txCtx, true);
    }
}

void ui_approve_tx_prev_screen(tx_ctx_t *txCtx) {
    if (formatter_index == -1) {
        MEMCLEAR(formatter_stack);
        formatter_index = 0;
        current_data_index--;
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
        MEMCLEAR(detailCaption);
        MEMCLEAR(detailValue);
        MEMCLEAR(opCaption);
        formatter_stack[formatter_index](&G_context.tx_info);

        if (opCaption[0] != '\0') {
            strlcpy(detailCaption, opCaption, sizeof(detailCaption));
            detailValue[0] = ' ';
        }
    }
}