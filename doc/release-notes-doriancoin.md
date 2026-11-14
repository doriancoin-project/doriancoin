Doriancoin Core version 0.21.83 is now available from:

 <https://github.com/doriancoin-project/doriancoin/releases/tag/v0.21.83>.

This release enables a consensus rule change (a soft fork) at mainnet height
1,411,850, expected around November 27, 2026, and backports a batch of MWEB
validation, relay and mempool fixes from Litecoin Core 0.21.5.7 and 0.21.5.8.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/doriancoin-project/doriancoin/issues>

Upgrade urgency
===============

**High — miners and pools must upgrade before height 1,411,850.**

The new rule only tightens what a valid block may contain, so every block an
upgraded miner produces is still accepted by older nodes. Nothing stops
working at the activation height, but the risk is not shared evenly:

- **Miners and pools: required.** A miner still on 0.21.82 can produce a block
  that upgraded nodes reject, and loses the reward for it.
- **Exchanges, explorers, and Electrum servers: strongly recommended.** A
  0.21.82 node will follow a block that breaks the new rule until the upgraded
  majority builds a longer chain, and may show transactions that are later
  reversed.
- **Everyone else: recommended.** The relay, mempool and block-handling fixes
  below take effect as soon as you upgrade, independent of the activation
  height.

Upgraded nodes report `/DoriancoinCore:0.21.83/` to their peers, so
`getpeerinfo` shows how much of the network has moved.

How to Upgrade
==============

Shut down the old version, wait until it has completely shut down (this may
take a few minutes on older machines), then run the installer (Windows) or copy
over `/Applications/Doriancoin-Qt` (macOS) or `doriancoind`/`doriancoin-qt`
(Linux).

**No reindex is required.** See "Pegout status in undo data" below for the one
narrow case where an operator may choose to run one anyway.

**Do not downgrade to 0.21.82 on the same data directory.** Once 0.21.83 has
connected blocks, their undo records use a format the older version misreads.
Going back needs a data directory backup taken before the upgrade, or a fresh
sync.

macOS: opening the app the first time
-------------------------------------

The `.dmg` is signed ad-hoc, not with an Apple Developer ID, and is therefore
not notarized. macOS will refuse to open it on the first attempt, reporting
that it "cannot be opened because Apple cannot check it for malicious
software". This is expected, and does not indicate a corrupt download --
verify the download against `SHA256SUMS` and then allow it:

- Open **System Settings > Privacy & Security**, scroll to the message about
  Doriancoin Core, and click **Open Anyway**; or
- from a terminal, `xattr -dr com.apple.quarantine /Applications/Doriancoin-Qt.app`.

Windows: SmartScreen on the installer
-------------------------------------

The installer is unsigned, so SmartScreen shows "Windows protected your PC"
and hides the button that proceeds. Click **More info**, then **Run anyway**.
As on macOS, this reflects the absence of a paid code-signing certificate
rather than anything wrong with the download; check it against `SHA256SUMS`.

Notable changes
===============

Consensus: MWEB extra-data feature activation at height 1,411,850
-----------------------------------------------------------------

An MWEB kernel that sets the extra-data feature bit must carry non-empty extra
data. Kernels that set the bit with an empty vector are rejected in blocks at
and after height 1,411,850 (`consensus.mweb_extradata_feature_activation_height`).
Testnet and regtest apply the rule from the first MWEB block.

This mirrors the pegout rule that activated at 1,363,000, which is unchanged.
The wallet has never produced such kernels and they were already non-standard,
so no ordinary transaction is affected.

Both empty-feature kernels are also refused at mempool admission regardless of
height, including on networks that accept non-standard transactions, so they
cannot reach a mining template.

  - `edbee80`: mweb: adapt upstream validation and relay hardening
  - `68a4885`: mweb: reject noncanonical kernel features at mempool admission

MWEB block validation hardening
-------------------------------

MWEB block bodies are not committed to by the block hash, so two different
bodies can share one hash. Several fixes close gaps around that:

- The exact body being connected is re-checked at connection time, including
  when it has been reloaded from disk.
- Kernel data is authenticated against the header the HogEx commits to before
  any feature bit is acted on, and a body that fails the empty-pegout or
  empty-extra-data check is treated as mutated rather than permanently
  invalidating a block hash that may be valid with a different body.
- Block storage and activation are serialised so one representation cannot be
  connected while another is kept on disk.
- Peers that send such bodies in compact blocks are now penalised.

  - `edbee80`: mweb: adapt upstream validation and relay hardening

Pegout status in undo data
--------------------------

Undo records, which restore spent outputs when a block is disconnected, did
not store whether an output was a pegout. A pegout restored by a
reorganisation therefore lost its 6-block maturity requirement. The flag is
now written with every undo record.

Records written by earlier versions cannot be repaired without replaying the
chain, but they matter only if a reorganisation disconnects a block that was
connected before the upgrade and the replacement chain re-spends one of its
pegouts while still immature. Those blocks are out of reach of any realistic
reorganisation within hours of upgrading, so **no reindex is needed**.
Operators who want every historical record regenerated can run once with
`-reindex` (`-reindex-chainstate` is not sufficient). Details are in
[`doc/upstream-backports-2026-11.md`](upstream-backports-2026-11.md).

  - `9bf23bc`: mempool: serialize fPegout in txundo

Mempool
-------

- `cfcf847`: MWEB transactions that conflict with a newly connected block --
  by sharing a kernel or spending an input the block spent -- are now evicted,
  together with descendants whose inputs no longer exist. Previously they
  could linger and produce invalid block templates.
- `6a71b4d`: the miner validates the kernel and stealth sums of the MWEB
  transactions staged for a block incrementally, and refuses duplicate
  kernels, so individually valid transactions that cannot be aggregated no
  longer break the template.

Networking
----------

- `edbee80`: a rejected MWEB transaction is no longer remembered in the
  recent-rejects filter. Its txid and wtxid do not commit to the MWEB payload,
  so an invalid variant could previously be used to stop a node from accepting
  the valid transaction with the same identifiers.
- `edbee80`: fast compact-block announcement of an MWEB block now happens
  after the block has connected instead of after the header check, because the
  MWEB body can only be validated against the MWEB UTXO set. Block propagation
  is marginally slower as a result.

RPC
---

- `edbee80`: `getmempoolentry` returns an error for an MWEB-only transaction
  when the node runs with `-rpcserialversion` 0 or 1, instead of returning an
  entry the client cannot interpret.

Fixes
-----

- `bc236cc`: precomputed transaction data now outlives the script-check queue
  on every early-return path in `ConnectBlock`
- `33266f9`: Bulletproof verification releases its scratch space when a proof
  contains a malformed point
- `14dd45d`: file descriptors are closed on all paths of the libmw file copy
- `8c606e6`: `InitCoinsDB` takes `cs_main` around its block-index lookup

Build and testing
-----------------

- `0459f64`, `a452d24`: out-of-tree builds find the libmw test headers and the
  generated secp256k1-zkp configuration
- `d4b5de1`: the functional test runner reports failures even when a test's
  captured output is not valid UTF-8
- New functional tests `mweb_newpowvalidblock.py`,
  `mweb_noncanonical_features.py`, `mweb_pegout_undo.py` and
  `mweb_wtxid_recentrejects.py`; activation-boundary and legacy undo
  serialisation unit tests

Documentation
-------------

- `acd42b2`: MWEB protocol documentation under `doc/mweb/` (consensus rules,
  kernels, serialisation, weight), with Doriancoin's activation heights

Chainparams
-----------

`nMinimumChainWork`, `defaultAssumeValid`, and `chainTxData` updated to block
1,403,000.

Credits
=======

Thanks to everyone who directly contributed to this release:

- [David Burkett](https://github.com/DavidBurkett/)
- [mboyd1](https://github.com/mboyd1)
- [xanimo](https://github.com/xanimo)

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/litecoin-project/litecoin/).
