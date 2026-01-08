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
    const int64_t T = params.nPowTargetSpacing;
    const int64_t N = params.nLWMAWindow;

    const arith_uint256 powLimit = UintToArith256(params.powLimit);
    const unsigned int nProofOfWorkLimit = powLimit.GetCompact();

    // Handle regtest no-retarget mode
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Special testnet rule: allow min-difficulty blocks
    if (params.fPowAllowMinDifficultyBlocks) {
        if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + T * 2)
            return nProofOfWorkLimit;
    }

    // Calculate available blocks since LWMA activation
    int64_t height = pindexLast->nHeight + 1;
    int64_t blocksAvailable = height - params.nLWMAHeight;

    // Use actual available blocks, minimum 1, maximum N
    int64_t actualN = std::min(N, std::max(blocksAvailable, (int64_t)1));

    // If insufficient history (need at least 2 blocks for a timespan), use previous difficulty
    if (actualN < 2) {
        return pindexLast->nBits;
    }

    // LWMA calculation
    arith_uint256 sumTarget;
    int64_t t = 0;

    const CBlockIndex* pindex = pindexLast;

    // In LWMA, weight should be highest for most recent blocks, lowest for oldest
    // We iterate from newest to oldest, so weight starts at actualN and decreases
    for (int64_t i = actualN; i > 0; i--) {
        const CBlockIndex* pindexPrev = pindex->pprev;
        if (!pindexPrev) break;

        int64_t solvetime = pindex->GetBlockTime() - pindexPrev->GetBlockTime();

        // Limit solvetime to prevent timestamp manipulation
        // Using 6*T bounds as recommended by Zawy
        if (solvetime < -6 * T) solvetime = -6 * T;
        if (solvetime > 6 * T) solvetime = 6 * T;

        // Weight 'i' gives highest weight (actualN) to newest block, lowest (1) to oldest
        t += solvetime * i;

        arith_uint256 target;
        target.SetCompact(pindex->nBits);
        sumTarget += target / actualN;

        pindex = pindexPrev;
    }

    // Prevent division by zero - set minimum weighted timespan
    if (t <= 0) t = 1;

    // LWMA formula: nextTarget = averageTarget * t / expectedT
    // Where expectedT = T * (1 + 2 + ... + N) = T * N * (N+1) / 2
    // Since sumTarget = sum(target_i / actualN) = averageTarget,
    // we have: nextTarget = sumTarget * t / (T * actualN * (actualN+1) / 2)
    int64_t actualK = actualN * (actualN + 1) * T / 2;
    arith_uint256 nextTarget = (sumTarget * t) / actualK;

    // Clamp to powLimit
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
