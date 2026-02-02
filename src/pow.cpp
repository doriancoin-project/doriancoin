// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <pow.h>

#include <arith_uint256.h>
#include <chain.h>
#include <primitives/block.h>
#include <uint256.h>

unsigned int GetNextWorkRequiredBTC(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // Doriancoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = params.DifficultyAdjustmentInterval()-1;
    if ((pindexLast->nHeight+1) != params.DifficultyAdjustmentInterval())
        blockstogoback = params.DifficultyAdjustmentInterval();

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Doriancoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespan;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

// LWMA - Linear Weighted Moving Average difficulty algorithm
// Copyright (c) 2017-2019 Zawy
// Reference: https://github.com/zawy12/difficulty-algorithms/issues/3
//
// Standard LWMA formula that weights solvetimes by recency.
// Newer blocks have higher weight, providing faster response to hashrate changes.
unsigned int GetNextWorkRequiredLWMA(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast);

    const int64_t T = params.nPowTargetSpacing;
    const int64_t N = params.nLWMAWindow;
    const arith_uint256 powLimit = UintToArith256(params.powLimit);

    // Handle regtest no-retarget mode
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Calculate how many blocks we can use since LWMA activation
    int64_t height = pindexLast->nHeight + 1;
    int64_t blocks = std::min<int64_t>(N, height - params.nLWMAHeight);

    // Need at least 3 blocks for a meaningful LWMA calculation
    if (blocks < 3)
        return pindexLast->nBits;

    // Get the previous block's target as our starting point
    arith_uint256 prevTarget;
    prevTarget.SetCompact(pindexLast->nBits);

    // LWMA calculation - weight solvetimes by position (newer = higher weight)
    int64_t sumWeightedSolvetimes = 0;
    int64_t sumWeights = 0;

    const CBlockIndex* block = pindexLast;

    // Iterate from newest to oldest
    // Weight 'i' goes from 'blocks' (newest, highest weight) down to 1 (oldest, lowest weight)
    for (int64_t i = blocks; i >= 1; i--) {
        const CBlockIndex* prev = block->pprev;
        if (!prev) break;

        int64_t solvetime = block->GetBlockTime() - prev->GetBlockTime();

        // Clamp solvetime: minimum 1 second (no zero/negative), maximum 6*T
        if (solvetime < 1) solvetime = 1;
        if (solvetime > 6 * T) solvetime = 6 * T;

        // Weight solvetime by position - newest block gets highest weight
        sumWeightedSolvetimes += solvetime * i;
        sumWeights += i;

        block = prev;
    }

    // Calculate expected weighted solvetime if all blocks were on-target
    int64_t expectedWeightedSolvetimes = sumWeights * T;

    // Safety: symmetric caps limit adjustment to 10x per block in either direction
    // This prevents both difficulty collapse (runaway easy) and spikes (runaway hard)
    int64_t minWeightedSolvetimes = expectedWeightedSolvetimes / 10;  // Max 10x difficulty increase
    int64_t maxWeightedSolvetimes = expectedWeightedSolvetimes * 10;  // Max 10x difficulty decrease

    if (sumWeightedSolvetimes < minWeightedSolvetimes)
        sumWeightedSolvetimes = minWeightedSolvetimes;
    if (sumWeightedSolvetimes > maxWeightedSolvetimes)
        sumWeightedSolvetimes = maxWeightedSolvetimes;

    // Standard LWMA formula:
    // nextTarget = prevTarget * (weightedAvgSolvetime / T)
    //            = prevTarget * sumWeightedSolvetimes / (sumWeights * T)
    //
    // This correctly:
    // - Decreases target (raises difficulty) when blocks are fast (ratio < 1)
    // - Increases target (lowers difficulty) when blocks are slow (ratio > 1)
    // - Maintains target when blocks are on-schedule (ratio = 1)
    arith_uint256 nextTarget = prevTarget * sumWeightedSolvetimes / expectedWeightedSolvetimes;

    // Clamp to powLimit (minimum difficulty)
    if (nextTarget > powLimit)
        nextTarget = powLimit;

    return nextTarget.GetCompact();
}

// LWMAv2 - Stabilized LWMA difficulty algorithm
// Fixes feedback loop instability by using window-start target as reference
// instead of previous block target, preventing compounding oscillations.
unsigned int GetNextWorkRequiredLWMAv2(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast);

    const int64_t T = params.nPowTargetSpacing;
    const int64_t N = params.nLWMAWindow;
    const arith_uint256 powLimit = UintToArith256(params.powLimit);

    // Handle regtest no-retarget mode
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Calculate how many blocks we can use since LWMA activation
    int64_t height = pindexLast->nHeight + 1;
    int64_t blocks = std::min<int64_t>(N, height - params.nLWMAHeight);

    // Need at least 3 blocks for a meaningful LWMA calculation
    if (blocks < 3)
        return pindexLast->nBits;

    // KEY FIX: Use target from START of window as reference (not previous block)
    // This breaks the feedback loop that caused oscillations in v1
    const CBlockIndex* windowStart = pindexLast;
    for (int64_t i = 0; i < blocks && windowStart->pprev; i++) {
        windowStart = windowStart->pprev;
    }
    arith_uint256 referenceTarget;
    referenceTarget.SetCompact(windowStart->nBits);

    // LWMA calculation - weight solvetimes by position (newer = higher weight)
    int64_t sumWeightedSolvetimes = 0;
    int64_t sumWeights = 0;

    const CBlockIndex* block = pindexLast;

    // Iterate from newest to oldest
    for (int64_t i = blocks; i >= 1; i--) {
        const CBlockIndex* prev = block->pprev;
        if (!prev) break;

        int64_t solvetime = block->GetBlockTime() - prev->GetBlockTime();

        // Clamp solvetime: minimum 1 second (no zero/negative), maximum 6*T
        if (solvetime < 1) solvetime = 1;
        if (solvetime > 6 * T) solvetime = 6 * T;

        // Weight solvetime by position - newest block gets highest weight
        sumWeightedSolvetimes += solvetime * i;
        sumWeights += i;

        block = prev;
    }

    // Calculate expected weighted solvetime if all blocks were on-target
    int64_t expectedWeightedSolvetimes = sumWeights * T;

    // KEY FIX: Tighter caps (3x instead of 10x) as safety valve
    // With window-start reference, extreme caps should rarely be hit
    int64_t minWeightedSolvetimes = expectedWeightedSolvetimes / 3;  // Max 3x difficulty increase
    int64_t maxWeightedSolvetimes = expectedWeightedSolvetimes * 3;  // Max 3x difficulty decrease

    if (sumWeightedSolvetimes < minWeightedSolvetimes)
        sumWeightedSolvetimes = minWeightedSolvetimes;
    if (sumWeightedSolvetimes > maxWeightedSolvetimes)
        sumWeightedSolvetimes = maxWeightedSolvetimes;

    // Apply adjustment to reference target (from window start, not previous block)
    arith_uint256 nextTarget = referenceTarget * sumWeightedSolvetimes / expectedWeightedSolvetimes;

    // Clamp to powLimit (minimum difficulty)
    if (nextTarget > powLimit)
        nextTarget = powLimit;

    return nextTarget.GetCompact();
}

// ASERT - Absolutely Scheduled Exponential Rise Target
// Based on BCH's aserti3-2d algorithm by Mark Lundeberg.
// Eliminates oscillation by computing difficulty from total time deviation
// relative to an ideal block schedule, using an exponential adjustment.
//
// For each block: target = anchor_target * 2^((time_delta - T * height_delta) / halflife)
//
// Properties:
// - Mathematically proven to never oscillate
// - No window lag - responds to each block individually
// - With constant hashrate, difficulty stays perfectly flat

// Cached anchor block pointer (set once, never changes after activation)
static const CBlockIndex* g_asert_anchor = nullptr;

void ResetASERTAnchorCache()
{
    g_asert_anchor = nullptr;
}

static const CBlockIndex* GetASERTAnchorBlock(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    // Return cached anchor if available
    const CBlockIndex* pindex = g_asert_anchor;
    if (pindex)
        return pindex;

    // Walk back to find the anchor block at nASERTHeight
    pindex = pindexLast;
    while (pindex->nHeight > params.nASERTHeight) {
        pindex = pindex->pprev;
        assert(pindex);
    }
    assert(pindex->nHeight == params.nASERTHeight);

    // Cache for future calls
    g_asert_anchor = pindex;
    return pindex;
}

unsigned int GetNextWorkRequiredASERT(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast);

    const arith_uint256 powLimit = UintToArith256(params.powLimit);

    // Handle regtest no-retarget mode
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Find the anchor block and its parent
    const CBlockIndex* pindexAnchor = GetASERTAnchorBlock(pindexLast, params);
    assert(pindexAnchor->pprev);

    // Anchor target: hardcoded to ~0.04 difficulty for equilibrium
    arith_uint256 anchorTarget;
    anchorTarget.SetCompact(params.nASERTAnchorBits);

    // Time delta: current block's parent timestamp minus anchor's parent timestamp
    // Using parent timestamps avoids manipulation of the current block's timestamp
    const int64_t anchorParentTime = pindexAnchor->pprev->GetBlockTime();
    const int64_t currentParentTime = pindexLast->GetBlockTime();
    const int64_t timeDelta = currentParentTime - anchorParentTime;

    // Height delta: height of block being computed minus anchor height
    const int64_t nHeight = pindexLast->nHeight + 1;
    const int64_t heightDelta = nHeight - params.nASERTHeight;

    const int64_t T = params.nPowTargetSpacing;
    const int64_t halfLife = params.nASERTHalfLife;

    // Compute exponent in fixed-point with 16 fractional bits:
    // exponent = (timeDelta - T * heightDelta) / halfLife
    // In fixed-point: exponent_fp = (timeDelta - T * heightDelta) * 65536 / halfLife
    const int64_t exponent = ((timeDelta - T * heightDelta) * int64_t(65536)) / halfLife;

    // Decompose into integer shifts and fractional part
    // We need: shifts (integer part) and frac in [0, 65536)
    int32_t shifts;
    uint16_t frac;

    if (exponent >= 0) {
        shifts = static_cast<int32_t>(exponent >> 16);
        frac = static_cast<uint16_t>(exponent & 0xFFFF);
    } else {
        // For negative exponents, ensure frac is in [0, 65536)
        // Example: -2.3 → shifts = -3, frac = 0.7 * 65536
        const int64_t absExponent = -exponent;
        shifts = -static_cast<int32_t>(absExponent >> 16);
        const uint16_t remainder = static_cast<uint16_t>(absExponent & 0xFFFF);
        if (remainder != 0) {
            shifts--;
            frac = 65536 - remainder;
        } else {
            frac = 0;
        }
    }

    // Compute 2^(frac/65536) * 65536 using cubic polynomial approximation
    // Coefficients from BCH aserti3-2d (designed to stay within uint64 bounds)
    // Approximates: 65536 * 2^(frac/65536) for frac in [0, 65535]
    uint32_t factor = 65536;
    if (frac > 0) {
        const uint64_t f = frac;
        factor = 65536 + static_cast<uint32_t>(
            (uint64_t(195766423245049) * f +
             uint64_t(971821376) * f * f +
             uint64_t(5127) * f * f * f +
             (uint64_t(1) << 47)) >> 48);
    }

    // Apply fractional part: target = anchorTarget * factor / 65536
    arith_uint256 nextTarget = anchorTarget * factor;
    nextTarget >>= 16;

    // Apply integer shifts (left shift = easier, right shift = harder)
    if (shifts > 0) {
        // Positive shifts: difficulty decreasing (target increasing)
        // Clamp to avoid overflow past powLimit
        if (shifts >= 256) {
            return powLimit.GetCompact();
        }
        nextTarget <<= shifts;
    } else if (shifts < 0) {
        // Negative shifts: difficulty increasing (target decreasing)
        const int32_t absShifts = -shifts;
        if (absShifts >= 256) {
            // Target would be essentially 0 - return maximum difficulty
            return arith_uint256(1).GetCompact();
        }
        nextTarget >>= absShifts;
    }

    // Ensure target is at least 1 (maximum possible difficulty)
    if (nextTarget == arith_uint256(0))
        nextTarget = arith_uint256(1);

    // Clamp to powLimit (minimum difficulty)
    if (nextTarget > powLimit)
        nextTarget = powLimit;

    return nextTarget.GetCompact();
}

// Main dispatch function - routes to appropriate algorithm based on block height
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    int nHeight = pindexLast->nHeight + 1;

    // Use ASERT algorithm after ASERT activation height
    if (nHeight > params.nASERTHeight) {
        return GetNextWorkRequiredASERT(pindexLast, pblock, params);
    }

    // Use stabilized LWMAv2 algorithm after fix height
    if (nHeight >= params.nLWMAFixHeight) {
        return GetNextWorkRequiredLWMAv2(pindexLast, pblock, params);
    }

    // Use original LWMA algorithm after activation height (but before fix)
    if (nHeight >= params.nLWMAHeight) {
        return GetNextWorkRequiredLWMA(pindexLast, pblock, params);
    }

    // Use original BTC-style algorithm before LWMA activation
    return GetNextWorkRequiredBTC(pindexLast, pblock, params);
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
