Doriancoin Core version 0.21.82 is now available from:

 <https://github.com/doriancoin-project/doriancoin/releases/tag/v0.21.82>.

This is a **mandatory** release. It fixes a consensus bug that halted the
mainnet chain at height 1,359,051, and it enables a consensus rule change at
height 1,363,000. Nodes running 0.21.80 or earlier cannot follow the chain past
1,359,051 and must upgrade.

Please report bugs using the issue tracker at GitHub:

  <https://github.com/doriancoin-project/doriancoin/issues>

Upgrade urgency
===============

**Critical — upgrade immediately.**

Two independent deadlines apply:

- **Height 1,359,051 (already passed).** The chain stopped here. Only 0.21.82
  computes the correct difficulty for the blocks that follow, so a 0.21.80 node
  will reject block 1,359,052 and stay stuck at the old tip.
- **Height 1,363,000.** New MWEB pegout rules begin enforcement at this height.
  Nodes that have not upgraded by then may accept blocks the rest of the
  network rejects.

Mining pools, exchanges, and explorers should upgrade before height 1,363,000.

How to Upgrade
==============

Shut down the old version, wait until it has completely shut down (this may
take a few minutes on older machines), then run the installer (Windows) or copy
over `/Applications/Doriancoin-Qt` (macOS) or `doriancoind`/`doriancoin-qt`
(Linux). No reindex or `-reindex-chainstate` is required: blocks below
1,359,052 validate bit-for-bit under the old rules.

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

Notable changes
===============

Consensus: ASERT 256-bit overflow fix
-------------------------------------

`GetNextWorkRequiredASERT` applied the integer part of the ASERT exponent with
`nextTarget <<= shifts`. `arith_uint256::operator<<=` silently discards bits
shifted past bit 255 and gives no overflow signal, so a large positive shift
wrapped the target to a *tiny* value — the hardest possible difficulty — when
the mathematically correct answer was the easiest.

The previous guard only rejected `shifts >= 256`, but the mainnet anchor target
is 229 bits wide, so bits begin falling off the top at a shift of 28. At a shift
of 48 the anchor's mantissa clears bit 255 entirely, the value becomes 0, and
the "target is at least 1" floor turns that into target 1.

Roughly 48 hours of lag behind the ASERT schedule was therefore enough to
permanently brick the chain, and that is what happened at height 1,359,051: the
chain was 573,873 seconds (6.64 days) behind schedule, the exponent produced a
shift of +159, the target wrapped, and `getblocktemplate` began serving an
unmineable `nBits` of `0x01010000`.

Overflow is now detected the way the BCH `aserti3-2d` reference this was derived
from does — shift back and compare. If bits were lost, the true value exceeds
`powLimit` anyway, so clamping is exactly equivalent to infinite precision.

  - `ccb69d7`: consensus: Fix ASERT 256-bit overflow that bricked the chain at 1359051

Consensus: ASERT re-anchor at height 1,359,052
----------------------------------------------

With the overflow fixed, ASERT correctly returns `powLimit` for the stalled tip,
because the chain genuinely was 6.64 days behind its absolute schedule. ASERT
would then repay that debt honestly — against the network's real hashrate that
is roughly 4,300 blocks over 6.6 hours, emitting some 108,000 DSV.

A second anchor is added instead, so the chain restarts with zero schedule debt:

    nASERT2Height     = 1359052   (the block that was unmineable)
    nASERT2AnchorBits = 0x1d027ffd (~0.4 difficulty)

Moving the original `nASERTHeight` would *not* have been a valid way to do this:
both the dispatcher and the anchor walk key off it, so changing it would
retroactively re-derive the difficulty of blocks 1,246,001–1,359,051 and
invalidate the entire existing chain. The first anchor is left untouched and
history validates bit-for-bit.

The new schedule's origin is the anchor block's own timestamp rather than its
parent's — the anchor's parent is the stalled tip, and inheriting that gap would
reproduce the very debt being cleared.

`nASERTHalfLife` remains 3600. This chain sees repeated 100x hashrate spikes,
and because ASERT responds to accumulated schedule deviation rather than elapsed
time, a longer halflife would let a spike miner extract proportionally more fast
blocks before difficulty caught up. The halflife was never the bug.

  - `6ea73cd`: consensus: Re-anchor ASERT at 1359052 to reset the stall debt

Consensus: MWEB pegout feature activation at height 1,363,000
-------------------------------------------------------------

MWEB kernels that signal a pegout must actually contain at least one pegout.
Empty pegout features are rejected at and after height 1,363,000
(`consensus.mweb_pegout_feature_activation_height`). MWEB kernel lock heights
are now enforced, and a known MWEB-rebalancing output is frozen.

  - `c9c1b3e`: validation: reject empty MWEB pegout features
  - `d0f2b71`: consensus: Start enforcing kernel lock height
  - `9019c89`: Consensus: Freeze MWEB-rebalancing output
  - `68023a9`: consensus: use Doriancoin heights for the MWEB soft forks

Consensus: BIP34 fixes
----------------------

The BIP34 coinbase height check is exempted for the genesis block, which
previously broke `-reindex` from scratch, and the testnet BIP34 parameters have
been corrected. Testnet has been reset to a working state.

  - `bccfab5`: consensus: exempt genesis from the BIP34 coinbase height check
  - `28b82fe`: consensus: correct testnet BIP34 parameters
  - `4f4befa`: consensus: reset testnet to a working state

MWEB security hardening
-----------------------

A batch of MWEB security fixes backported from Litecoin Core, covering block
mutation, duplicate pegins, MMR durability, and P2P resource limits:

  - `dfccd41`: validation: preserve descendants of mutated MWEB blocks
  - `3752878`: Ensure mutated MWEB block doesn't get flagged as BLOCK_CONSENSUS when including frozen output ID
  - `8bdbab4`: Avoid mutating CoinsView state with invalid block
  - `14de4c8`: Treat hogex fee and amount mismatches as BLOCK_CONSENSUS errors
  - `c03e824`: Verify no HogEx flag is set pre-MWEB activation
  - `87850dd`: Add additional guards against duplicate pegins
  - `764ae11`: Erase block data for mutated blocks
  - `1b88564`: Prevent kernel fee overflow
  - `3ee2dcb`: Fix data corruption issue on PMMR rewind
  - `86bc144`: Improve file write durability for MMRs
  - `7d8ec3f`: Open file with update attribute in Commit
  - `9adeaf0`: Belt-and-suspenders input commitment and pubkey checks
  - `26c7ac2`: Adding fallbacks for Hash-to-SecretKey edge cases
  - `19a58dc`: Fix cache leaf bounds check
  - `f86e1e9`: Update MWEB chainstate in RollforwardBlock

Policy and mempool
------------------

- `d61f760`: `MAX_STANDARD_TX_WEIGHT` raised from 400,000 to 2,180,000, allowing
  larger transactions (for example large consolidations) to relay and be mined.
  This is a policy-only change.
- `13f33d7`: limit MWEB weight for mempool transactions
- `dfe0a25`: reject empty MWEB pegout kernels from standard policy
- `68f46ec`: enforce Ke and Ko pubkey validity
- `0557c1a`: enforce `IsStandard` for pegout scripts
- `9834668`: enforce the MWEB input limit in the miner
- `82425bd`: support `maxfeerate=0` for MWEB transactions in `sendrawtransaction`
  and `testmempoolaccept`

Wallet: minimal Taproot (P2TR) support
--------------------------------------

`f3bbdd2` adds bech32m address generation and Taproot-aware transaction signing,
enough for the `ord` ordinals indexer to create inscriptions on Doriancoin.
`getnewaddress` and `getrawchangeaddress` now accept `"bech32m"`.

This is deliberately minimal. The wallet funds P2TR outputs and preserves
Taproot witnesses produced by an external signer, but it does not itself perform
BIP 341 key-path signing, and `bech32m` is excluded from change types and from
`GetAllDestinationsForKey` to avoid keypool exhaustion.

Networking
----------

- `0e70f63`: `MAX_PROTOCOL_MESSAGE_LENGTH` increased to 32MB
- `0e9cf69`: discourage mutated MWEB compact blocks
- `5dced16`, `1cd9668`: MWEB leafset requests are rate-limited node-wide

Build and testing
-----------------

- `598221b`: GitHub Actions CI replaces the stale Travis and Cirrus configs; the
  full unit and functional suites now run on every push and pull request
- `ac70dc1`: functional test timeouts scale on CI runners via `--timeout-factor`
- `0945cd4`: pure-Python RIPEMD-160 for the functional test framework
- `77488f7`: `doriancoin_scrypt` provided to the functional test framework
- `2436b39`, `8e333e0`: new `-mwebheight` and `-taprootheight` regtest options
- `82c094d`: the difficulty bits test is fixed and registered
- Unit and functional test vectors updated for Doriancoin's parameters

Chainparams
-----------

`nMinimumChainWork`, `defaultAssumeValid`, and `chainTxData` updated to block
1,360,000.

Credits
=======

Thanks to everyone who directly contributed to this release:

- [David Burkett](https://github.com/DavidBurkett/)
- [mboyd1](https://github.com/mboyd1)

As well as to everyone that helped with translations on
[Transifex](https://www.transifex.com/litecoin-project/litecoin/).
