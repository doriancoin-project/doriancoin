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

// Main dispatch function - routes to appropriate algorithm based on block height
unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);

    int nHeight = pindexLast->nHeight + 1;

    // Use LWMA algorithm after activation height
    if (nHeight >= params.nLWMAHeight) {
        return GetNextWorkRequiredLWMA(pindexLast, pblock, params);
    }

    // Use original BTC-style algorithm before activation
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
