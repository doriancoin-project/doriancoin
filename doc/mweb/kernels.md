# Kernels

Adapted from [Litecoin Core's MWEB documentation](https://github.com/litecoin-project/litecoin/commit/b4d7e70afc7d50b97abf70e5cf6ea401ee8ad08e) for Doriancoin.

## Current Features

All kernels begin with a single byte which acts as a bitvector to specify the "features" of the kernel.
The following feature bits are currently supported, though more could be added in the future:

* `FEE_FEATURE_BIT` = `0x01`
* `PEGIN_FEATURE_BIT` = `0x02`
* `PEGOUT_FEATURE_BIT` = `0x04`
* `HEIGHT_LOCK_FEATURE_BIT` = `0x08`
* `STEALTH_EXCESS_FEATURE_BIT` = `0x10`
* `EXTRA_DATA_FEATURE_BIT` = `0x20`

For each feature bit that's set, the corresponding feature rules apply to that kernel.

***NOTE**: Only `FEE_FEATURE_BIT`, `PEGIN_FEATURE_BIT`, `PEGOUT_FEATURE_BIT`, `HEIGHT_LOCK_FEATURE_BIT`, and `STEALTH_EXCESS_FEATURE_BIT` are considered "standard" features.
Transactions specifying the `EXTRA_DATA_FEATURE_BIT` or any higher feature bits are unlikely to be forwarded along the p2p network, nor are they likely to be included by most miners.
This may change in future releases.*

#### Fee

Kernels specifying the `FEE_FEATURE_BIT` shall contain a (non-negative `var_int`) fee field.
The fee value shall be deducted from the total MWEB supply, and may be collected by the miner of the containing block as part of its coinbase reward.

#### Peg-In

Kernels specifying the `PEGIN_FEATURE_BIT` shall contain a (non-negative `var_int`) peg-in amount field.
The peg-in amount shall be added to the total MWEB supply, and must have a corresponding pegin output on the canonical Doriancoin (DSV) chain.

#### Peg-Out

Kernels specifying the `PEGOUT_FEATURE_BIT` contain a vector of pegouts, each consisting of a non-negative `var_int` amount and a non-empty Doriancoin `scriptPubKey`. Standard transaction policy additionally checks each pegout script with `IsStandard`; the consensus parser does not impose a 4-42 byte script limit. On mainnet, the vector must be non-empty at and after height **1,363,000**.
The peg-out amount shall be deducted from the total MWEB supply by the miner.
The miner shall add an output to the integrating transaction (`HogEx`) for each pegout specified. The output's `amount` and `scriptPubKey` shall match the one specified in the kernel.

#### Height Lock

Kernels specifying the `HEIGHT_LOCK_FEATURE_BIT` shall contain a (non-negative `var_int`) lock height field.
For Doriancoin blocks strictly above height **1,363,000**, the kernel shall not be included in any block earlier than the lock height specified. The grandfather boundary is retained to avoid rejecting historical blocks; it is separate from the pegout-feature activation at the same height.

#### Stealth Excess

Kernels specifying the `STEALTH_EXCESS_FEATURE_BIT` shall contain a (compressed public key) stealth excess (`E'`) field.
The stealth excess will also be used when signing the kernel.

#### Extra Data

Kernels specifying the `EXTRA_DATA_FEATURE_BIT` encode a CompactSize byte count followed by that many data bytes. This is a serialized byte vector, not a one-byte length with a 100-byte limit; its size contributes to the MWEB weight limit.

At and after Doriancoin mainnet height **1,411,850**, the vector must be non-empty when this feature is set. Testnet and regtest enforce this rule whenever MWEB is active. The contents otherwise remain reserved for future soft forks. Extra-data kernels remain nonstandard under the current relay policy. See [consensus.md](./consensus.md#doriancoin-activation) for the separate activation rules.

## Signature

The kernel shall contain a valid signature for the kernel commitment (`E`). The signed message is the BLAKE3 hash of the feature byte followed by the excess, and then the included feature fields in the order fee, pegin amount, pegout vector, lock height, stealth excess, extra-data vector. Vectors include their CompactSize length prefixes. This order differs from wire serialization, which places the excess after the feature fields.

*Example: If the kernel's feature byte is 0x03 (`FEE_FEATURE_BIT` | `PEGIN_FEATURE_BIT`), the message signed shall be: `(0x03 | excess | VARINT(fee) | VARINT(pegin_amount))`*

If the `STEALTH_EXCESS_FEATURE_BIT` is set, instead of signing the kernel with just the kernel excess (`E`), it shall be signed with `BLAKE3(E||E')*E + E'`, using the hash-to-scalar conversion described in [consensus.md](./consensus.md#cryptographic-assumptions).

## Soft Forks

New features may be defined in the future via softfork. Consensus rules may be agreed upon for the currently undefined feature bits.
Though new data sections cannot be added without hardforking, future feature bits may be used in conjunction with the `EXTRA_DATA_FEATURE_BIT` to include additional kernel data.
