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

    // LWMA calculation with properly weighted targets AND solvetimes
    arith_uint256 sumWeightedTarget = 0;
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

        arith_uint256 target;
        target.SetCompact(block->nBits);

        // Weight both target and solvetime by position
        // Newest block gets weight 'blocks', oldest gets weight '1'
        sumWeightedTarget += target * i;
        sumWeightedSolvetimes += solvetime * i;
        sumWeights += i;

        block = prev;
    }

    // Safety: prevent extreme difficulty spikes from timestamp manipulation
    // Minimum weighted solvetime is 10% of expected (limits difficulty increase to 10x)
    int64_t minWeightedSolvetimes = sumWeights * T / 10;
    if (sumWeightedSolvetimes < minWeightedSolvetimes)
        sumWeightedSolvetimes = minWeightedSolvetimes;

    // LWMA formula for targets:
    // nextTarget = avgTarget * (avgSolvetime / expectedSolvetime)
    //            = (sumWeightedTarget / sumWeights) * (sumWeightedSolvetimes / sumWeights) / T
    //            = sumWeightedTarget * sumWeightedSolvetimes / (sumWeights^2 * T)
    //
    // This correctly:
    // - Decreases target (raises difficulty) when blocks are fast
    // - Increases target (lowers difficulty) when blocks are slow
    // - Maintains target when blocks are on-schedule
    int64_t denominator = sumWeights * sumWeights * T;
    arith_uint256 nextTarget = (sumWeightedTarget * sumWeightedSolvetimes) / denominator;

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
