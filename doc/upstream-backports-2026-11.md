# November 2026 upstream backports — released in 0.21.83

This document describes the Doriancoin integration of selected Litecoin changes through upstream commit [`9fe73a4bd`](https://github.com/litecoin-project/litecoin/commit/9fe73a4bd1433f9b90ee0ea5fc6e985b6702d343). These changes first ship in Doriancoin Core 0.21.83; see the [release notes](release-notes-doriancoin.md) for the summary. Doriancoin's difficulty algorithms, existing activation heights, and chain identity are retained.

## Mainnet activation

The new MWEB rule rejects kernels that set the extra-data feature bit but contain an empty extra-data vector. It activates at mainnet height **1,411,850**. Testnet and regtest configure this rule at height zero, so it applies whenever MWEB is active; regtest still requires explicit MWEB activation with `-mwebheight`.

The mainnet height was selected from the November 13, 2026 explorer snapshot at **1,403,786**, plus **8,064 blocks**. At the 150-second target spacing this is 1,209,600 seconds, or 14 days, giving an estimated **November 27, 2026** activation. Activation is determined by block height, not the calendar; actual block production can move that date. Operators and miners should upgrade before the activation height.

The existing empty-pegout rule remains active at and after **1,363,000**. The existing kernel-lock grandfather boundary also remains **1,363,000**, with block lock-height enforcement strictly above that boundary. The backport does not move either threshold to a Litecoin height.

Mempool admission also rejects empty pegout and extra-data feature vectors when nonstandard transactions are accepted. This closes a testnet mining-template failure found during integration review. It is an admission-policy check; historical block validation retains the activation heights above.

## Existing data directories and pegout undo

Undo records restore spent outputs when a block is disconnected. Older Doriancoin binaries omitted the `fPegout` flag from these records, so a disk-based disconnect could restore a spent pegout without its maturity classification. The new format preserves that flag in bit 31 of the encoded height/coinbase metadata.

The new reader accepts old undo bytes, but cannot reconstruct a flag that was never recorded. This affects **undo for pegout spends in blocks connected before the upgrade**, not UTXOs: the chainstate format already stores `fPegout` for unspent coins, and every block connected by the upgraded binary writes undo with the flag.

**No reindex is required.** Undo is read only when a block is disconnected, so an old record matters only if all of the following happen:

- a reorganisation disconnects a block that was connected *before* the upgrade,
- that block spent a pegout output created at or below the fork point, and
- the replacement chain spends the same output again within `PEGOUT_MATURITY` (6 blocks) of its creation, which no standard mempool relays.

Blocks connected before the upgrade are buried under new ones within hours, after which a reorganisation can no longer reach them. Should the case occur anyway, the affected node accepts one block its peers reject and returns to the valid chain as soon as that chain has more work; nothing is marked permanently invalid.

Operators who nevertheless want every historical undo record regenerated — for instance before using `invalidateblock` deep into history — can cleanly shut down, back up wallets, and start once with **`-reindex`**. `-reindex-chainstate` does not do this: it retains block-index undo positions, and `WriteUndoDataForBlock` does not rewrite records whose undo position already exists. A pruned node has to redownload block history to complete a `-reindex`.

Do not reuse new-format undo with an old binary. Its decoder treats bit 31 as part of the height, restoring a flagged coin at a height increased by **2^30** instead of recovering its pegout status. Any downgrade requires a compatible pre-upgrade data backup or a separately rebuilt data directory, as well as consideration of the active consensus rules. Wallet backups alone do not replace the block-index and undo data.

## Integrated upstream changes

The original commits below retain Litecoin attribution. Changes touching activation, test fixtures, or program names are adapted to Doriancoin.

| Upstream commit | Change |
|---|---|
| [`a399fc4ca`](https://github.com/litecoin-project/litecoin/commit/a399fc4ca73df7418e435d52a993eca44caf0475) | Reject empty MWEB kernel extra-data features at the Doriancoin activation height. |
| [`22a916236`](https://github.com/litecoin-project/litecoin/commit/22a9162363b5b713c529c3888bafd1a9ca8efb8c) | Preserve pegout status in transaction-input undo records. |
| [`32413709d`](https://github.com/litecoin-project/litecoin/commit/32413709d2902a5c7829a74d4955de57783d9a75) | Keep transaction data alive until queued script checks finish. |
| [`d3b9ac396`](https://github.com/litecoin-project/litecoin/commit/d3b9ac3969c20424e613c20b4726945976ef0cb6) | Reject `getmempoolentry` requests for MWEB-only transactions when RPC serialization is version 0 or 1. |
| [`b4313c34d`](https://github.com/litecoin-project/litecoin/commit/b4313c34d27ba1a462a478ddb83aa2fd7d05b5df) | Recheck the exact MWEB body being connected, including bodies reloaded from disk. |
| [`d7ab03d31`](https://github.com/litecoin-project/litecoin/commit/d7ab03d31c2272063a4580f15114406f6a7814ab) | Release Bulletproof scratch frames after malformed-point failures. |
| [`70ea696f3`](https://github.com/litecoin-project/litecoin/commit/70ea696f361cc78fe8e299c57980399ce333a9d2) | Authenticate kernel data and preserve valid alternatives after same-hash block mutations. |
| [`f6afd2682`](https://github.com/litecoin-project/litecoin/commit/f6afd2682620a1f331838c45e4d1f154b9656c90) | Validate staged mining aggregates incrementally, including kernel and stealth sums. |
| [`36f06455f`](https://github.com/litecoin-project/litecoin/commit/36f06455f9ef37f9973c5d8cc7af509ba967f959) | Evict MWEB mempool conflicts when a block connects. |
| [`c07d6228c`](https://github.com/litecoin-project/litecoin/commit/c07d6228c00cd0fb7ddb345bdf6b8711ff77650d) | Prevent rejected alternate MWEB payloads from suppressing valid transactions through the rejection cache. |
| [`0cb141964`](https://github.com/litecoin-project/litecoin/commit/0cb14196497bdb8c36447f52c85044282b81a857) | Defer MWEB fast relay until block connection succeeds. |
| [`b8ccaf355`](https://github.com/litecoin-project/litecoin/commit/b8ccaf3552a972e206d6ce161c1a9be0a2a83ed3) | Close POSIX file-copy descriptors, including failed-copy paths. |
| [`88c6e26c3`](https://github.com/litecoin-project/litecoin/commit/88c6e26c3f90a1a5e25cf5b744a7af7a3f66c40c) | Report failed functional tests even when captured output contains invalid UTF-8. |
| Build hunk from [`940c89ad7`](https://github.com/litecoin-project/litecoin/commit/940c89ad7190d49383f70f17f0b8c8e82f310b50) | Resolve the libmw test-header include path relative to `$(srcdir)` for out-of-tree builds; omit Litecoin version metadata. |
| [`b4d7e70af`](https://github.com/litecoin-project/litecoin/commit/b4d7e70afc7d50b97abf70e5cf6ea401ee8ad08e) | Add and update [MWEB protocol documentation](mweb/consensus.md), with Doriancoin activation rules and corrections checked against the implementation. |

The storage behavior described above is implemented in `src/undo.h`, `src/coins.h`, and `WriteUndoDataForBlock` in `src/validation.cpp`. Activation parameters are in `src/chainparams.cpp` and `src/libmw/include/mw/consensus/Params.h`.
