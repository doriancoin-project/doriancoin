// Copyright (c) 2015-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(pow_tests, BasicTestingSetup)

/* Test calculation of next difficulty target with no constraints applying */
BOOST_AUTO_TEST_CASE(get_next_work)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1358118740; // Block #30240
    CBlockIndex pindexLast;
    pindexLast.nHeight = 280223;
    pindexLast.nTime = 1358378777;  // Block #280223
    pindexLast.nBits = 0x1c0ac141;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1c093f8dU);
}

/* Test the constraint on the upper bound for next work */
BOOST_AUTO_TEST_CASE(get_next_work_pow_limit)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1317972665; // Block #0
    CBlockIndex pindexLast;
    pindexLast.nHeight = 2015;
    pindexLast.nTime = 1318480354;  // Block #2015
    pindexLast.nBits = 0x1e0ffff0;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1e0fffffU);
}

/* Test the constraint on the lower bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_lower_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1401682934; // Block #66528
    CBlockIndex pindexLast;
    pindexLast.nHeight = 578591;
    pindexLast.nTime = 1401757934;  // Block #578591
    pindexLast.nBits = 0x1b075cf1;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1b01d73cU);
}

/* Test the constraint on the upper bound for actual time taken */
BOOST_AUTO_TEST_CASE(get_next_work_upper_limit_actual)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    int64_t nLastRetargetTime = 1463690315; // NOTE: Not an actual block time
    CBlockIndex pindexLast;
    pindexLast.nHeight = 1001951;
    pindexLast.nTime = 1464900315;  // Block #46367
    pindexLast.nBits = 0x1b015318;
    BOOST_CHECK_EQUAL(CalculateNextWorkRequired(&pindexLast, nLastRetargetTime, chainParams->GetConsensus()), 0x1b054c60U);
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_negative_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    nBits = UintToArith256(consensus.powLimit).GetCompact(true);
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_overflow_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits = ~0x00800000;
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_too_easy_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 nBits_arith = UintToArith256(consensus.powLimit);
    nBits_arith *= 2;
    nBits = nBits_arith.GetCompact();
    hash.SetHex("0x1");
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_biger_hash_than_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith = UintToArith256(consensus.powLimit);
    nBits = hash_arith.GetCompact();
    hash_arith *= 2; // hash > nBits
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(CheckProofOfWork_test_zero_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    uint256 hash;
    unsigned int nBits;
    arith_uint256 hash_arith{0};
    nBits = hash_arith.GetCompact();
    hash = ArithToUint256(hash_arith);
    BOOST_CHECK(!CheckProofOfWork(hash, nBits, consensus));
}

BOOST_AUTO_TEST_CASE(GetBlockProofEquivalentTime_test)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    std::vector<CBlockIndex> blocks(10000);
    for (int i = 0; i < 10000; i++) {
        blocks[i].pprev = i ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = i;
        blocks[i].nTime = 1269211443 + i * chainParams->GetConsensus().nPowTargetSpacing;
        blocks[i].nBits = 0x207fffff; /* target 0x7fffff000... */
        blocks[i].nChainWork = i ? blocks[i - 1].nChainWork + GetBlockProof(blocks[i - 1]) : arith_uint256(0);
    }

    for (int j = 0; j < 1000; j++) {
        CBlockIndex *p1 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p2 = &blocks[InsecureRandRange(10000)];
        CBlockIndex *p3 = &blocks[InsecureRandRange(10000)];

        int64_t tdiff = GetBlockProofEquivalentTime(*p1, *p2, *p3, chainParams->GetConsensus());
        BOOST_CHECK_EQUAL(tdiff, p1->GetBlockTime() - p2->GetBlockTime());
    }
}

void sanity_check_chainparams(const ArgsManager& args, std::string chainName)
{
    const auto chainParams = CreateChainParams(args, chainName);
    const auto consensus = chainParams->GetConsensus();

    // hash genesis is correct
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, chainParams->GenesisBlock().GetHash());

    // target timespan is an even multiple of spacing
    BOOST_CHECK_EQUAL(consensus.nPowTargetTimespan % consensus.nPowTargetSpacing, 0);

    // genesis nBits is positive, doesn't overflow and is lower than powLimit
    arith_uint256 pow_compact;
    bool neg, over;
    pow_compact.SetCompact(chainParams->GenesisBlock().nBits, &neg, &over);
    BOOST_CHECK(!neg && pow_compact != 0);
    BOOST_CHECK(!over);
    BOOST_CHECK(UintToArith256(consensus.powLimit) >= pow_compact);

    // check max target * 4*nPowTargetTimespan doesn't overflow -- see pow.cpp:CalculateNextWorkRequired()
    /* Doriancoin: we allow overflowing by 1 bit
    if (!consensus.fPowNoRetargeting) {
        arith_uint256 targ_max("0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");
        targ_max /= consensus.nPowTargetTimespan*4;
        BOOST_CHECK(UintToArith256(consensus.powLimit) < targ_max);
    }
    */
}

BOOST_AUTO_TEST_CASE(ChainParams_MAIN_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::MAIN);
}

BOOST_AUTO_TEST_CASE(ChainParams_REGTEST_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::REGTEST);
}

BOOST_AUTO_TEST_CASE(ChainParams_TESTNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::TESTNET);
}

BOOST_AUTO_TEST_CASE(ChainParams_SIGNET_sanity)
{
    sanity_check_chainparams(*m_node.args, CBaseChainParams::SIGNET);
}

/* Test that dispatch uses BTC algorithm before LWMA activation */
BOOST_AUTO_TEST_CASE(lwma_dispatch_before_activation)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const Consensus::Params& params = chainParams->GetConsensus();

    // Create a block before LWMA activation height
    CBlockIndex pindexLast;
    pindexLast.nHeight = params.nLWMAHeight - 2; // One block before activation
    pindexLast.nTime = 1394325760;
    pindexLast.nBits = 0x1e0ffff0;
    pindexLast.pprev = nullptr;

    CBlockHeader header;
    header.nTime = pindexLast.nTime + params.nPowTargetSpacing;

    // Before activation, GetNextWorkRequired should use BTC algorithm
    // For non-retarget blocks, BTC algorithm returns previous block's nBits
    unsigned int result = GetNextWorkRequired(&pindexLast, &header, params);
    unsigned int btcResult = GetNextWorkRequiredBTC(&pindexLast, &header, params);
    BOOST_CHECK_EQUAL(result, btcResult);
}

/* Test that dispatch uses LWMA algorithm after activation */
BOOST_AUTO_TEST_CASE(lwma_dispatch_after_activation)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Temporarily set a low activation height for testing
    params.nLWMAHeight = 100;
    params.nLWMAWindow = 45;

    // Create chain of blocks after LWMA activation
    const int numBlocks = 50;
    std::vector<CBlockIndex> blocks(numBlocks);

    for (int i = 0; i < numBlocks; i++) {
        blocks[i].pprev = i > 0 ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = params.nLWMAHeight + i;
        blocks[i].nTime = 1394325760 + i * params.nPowTargetSpacing;
        blocks[i].nBits = 0x1e0ffff0;
    }

    CBlockHeader header;
    header.nTime = blocks[numBlocks - 1].nTime + params.nPowTargetSpacing;

    // After activation, GetNextWorkRequired should use LWMA algorithm
    unsigned int result = GetNextWorkRequired(&blocks[numBlocks - 1], &header, params);
    unsigned int lwmaResult = GetNextWorkRequiredLWMA(&blocks[numBlocks - 1], &header, params);
    BOOST_CHECK_EQUAL(result, lwmaResult);
}

/* Test LWMA cold start - insufficient history at activation */
BOOST_AUTO_TEST_CASE(lwma_cold_start)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Set activation height for testing
    params.nLWMAHeight = 100;
    params.nLWMAWindow = 45;

    // Create just 2 blocks after activation (minimal history)
    std::vector<CBlockIndex> blocks(3);

    // Block before activation
    blocks[0].pprev = nullptr;
    blocks[0].nHeight = params.nLWMAHeight - 1;
    blocks[0].nTime = 1394325760;
    blocks[0].nBits = 0x1e0ffff0;

    // First LWMA block
    blocks[1].pprev = &blocks[0];
    blocks[1].nHeight = params.nLWMAHeight;
    blocks[1].nTime = blocks[0].nTime + params.nPowTargetSpacing;
    blocks[1].nBits = 0x1e0ffff0;

    // Second LWMA block
    blocks[2].pprev = &blocks[1];
    blocks[2].nHeight = params.nLWMAHeight + 1;
    blocks[2].nTime = blocks[1].nTime + params.nPowTargetSpacing;
    blocks[2].nBits = 0x1e0ffff0;

    CBlockHeader header;
    header.nTime = blocks[2].nTime + params.nPowTargetSpacing;

    // With only 2 blocks of LWMA history, algorithm should still work
    // and use available blocks (graceful cold start)
    unsigned int result = GetNextWorkRequiredLWMA(&blocks[2], &header, params);

    // Result should be valid (not zero, not overflow)
    BOOST_CHECK(result != 0);
    arith_uint256 target;
    bool neg, over;
    target.SetCompact(result, &neg, &over);
    BOOST_CHECK(!neg && !over);
}

/* Test LWMA with exactly 1 block of history - should return previous difficulty */
BOOST_AUTO_TEST_CASE(lwma_single_block_history)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Set activation height for testing
    params.nLWMAHeight = 100;
    params.nLWMAWindow = 45;

    // Create single block at activation height
    CBlockIndex block0;
    block0.pprev = nullptr;
    block0.nHeight = params.nLWMAHeight - 1;
    block0.nTime = 1394325760;
    block0.nBits = 0x1e0ffff0;

    CBlockIndex block1;
    block1.pprev = &block0;
    block1.nHeight = params.nLWMAHeight;
    block1.nTime = block0.nTime + params.nPowTargetSpacing;
    block1.nBits = 0x1e0ffff0;

    CBlockHeader header;
    header.nTime = block1.nTime + params.nPowTargetSpacing;

    // With only 1 block of LWMA history (insufficient for timespan calc),
    // should return previous block's difficulty
    unsigned int result = GetNextWorkRequiredLWMA(&block1, &header, params);
    BOOST_CHECK_EQUAL(result, block1.nBits);
}

/* Test LWMA solvetime clamping */
BOOST_AUTO_TEST_CASE(lwma_solvetime_bounds)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Set activation height for testing
    params.nLWMAHeight = 100;
    params.nLWMAWindow = 10; // Smaller window for easier testing

    // Create blocks with extreme timestamps
    const int numBlocks = 15;
    std::vector<CBlockIndex> blocks(numBlocks);

    for (int i = 0; i < numBlocks; i++) {
        blocks[i].pprev = i > 0 ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = params.nLWMAHeight + i;
        // Every other block has extreme solvetime (10x target)
        if (i > 0 && i % 2 == 0) {
            blocks[i].nTime = blocks[i - 1].nTime + params.nPowTargetSpacing * 10;
        } else if (i > 0) {
            blocks[i].nTime = blocks[i - 1].nTime + params.nPowTargetSpacing;
        } else {
            blocks[i].nTime = 1394325760;
        }
        blocks[i].nBits = 0x1e0ffff0;
    }

    CBlockHeader header;
    header.nTime = blocks[numBlocks - 1].nTime + params.nPowTargetSpacing;

    // LWMA should handle extreme solvetimes without producing invalid results
    unsigned int result = GetNextWorkRequiredLWMA(&blocks[numBlocks - 1], &header, params);

    // Result should be valid (not zero, not overflow)
    BOOST_CHECK(result != 0);
    arith_uint256 target;
    bool neg, over;
    target.SetCompact(result, &neg, &over);
    BOOST_CHECK(!neg && !over);

    // Result should be within powLimit
    BOOST_CHECK(target <= UintToArith256(params.powLimit));
}

/* Test that dispatch uses LWMAv2 after fix activation height */
BOOST_AUTO_TEST_CASE(lwmav2_dispatch_after_fix_height)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Set activation heights for testing
    params.nLWMAHeight = 100;
    params.nLWMAFixHeight = 150;
    params.nLWMAWindow = 45;

    // Create chain of blocks after LWMAv2 fix activation
    const int numBlocks = 60;
    std::vector<CBlockIndex> blocks(numBlocks);

    for (int i = 0; i < numBlocks; i++) {
        blocks[i].pprev = i > 0 ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = params.nLWMAFixHeight + i;
        blocks[i].nTime = 1394325760 + i * params.nPowTargetSpacing;
        blocks[i].nBits = 0x1e0ffff0;
    }

    CBlockHeader header;
    header.nTime = blocks[numBlocks - 1].nTime + params.nPowTargetSpacing;

    // After fix height, GetNextWorkRequired should use LWMAv2 algorithm
    unsigned int result = GetNextWorkRequired(&blocks[numBlocks - 1], &header, params);
    unsigned int lwmav2Result = GetNextWorkRequiredLWMAv2(&blocks[numBlocks - 1], &header, params);
    BOOST_CHECK_EQUAL(result, lwmav2Result);
}

/* Test that LWMAv2 uses window-start target as reference (not previous block) */
BOOST_AUTO_TEST_CASE(lwmav2_uses_window_start_target)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Set activation heights for testing
    params.nLWMAHeight = 100;
    params.nLWMAFixHeight = 150;
    params.nLWMAWindow = 10;

    // Create a chain where the last few blocks have different difficulty
    // than the window start - this tests that v2 uses window-start reference
    const int numBlocks = 15;
    std::vector<CBlockIndex> blocks(numBlocks);

    for (int i = 0; i < numBlocks; i++) {
        blocks[i].pprev = i > 0 ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = params.nLWMAFixHeight + i;
        blocks[i].nTime = 1394325760 + i * params.nPowTargetSpacing; // On-target timing

        // First 5 blocks have base difficulty, last blocks have 10x harder difficulty
        // In v1 (using prevTarget), result would be based on hard difficulty
        // In v2 (using window-start), result should be based on window-start difficulty
        if (i < 5) {
            blocks[i].nBits = 0x1e0ffff0; // Base difficulty
        } else {
            blocks[i].nBits = 0x1d0ffff0; // 16x harder difficulty
        }
    }

    CBlockHeader header;
    header.nTime = blocks[numBlocks - 1].nTime + params.nPowTargetSpacing;

    // With on-target solvetimes, v2 should produce result based on window-start difficulty
    // not the recent blocks' harder difficulty
    unsigned int result = GetNextWorkRequiredLWMAv2(&blocks[numBlocks - 1], &header, params);

    // Result should be valid
    BOOST_CHECK(result != 0);
    arith_uint256 target;
    bool neg, over;
    target.SetCompact(result, &neg, &over);
    BOOST_CHECK(!neg && !over);

    // Since timing is on-target, result should be close to window-start difficulty (0x1e0ffff0)
    // not recent blocks' difficulty (0x1d0ffff0)
    arith_uint256 windowStartTarget;
    windowStartTarget.SetCompact(0x1e0ffff0);

    arith_uint256 recentTarget;
    recentTarget.SetCompact(0x1d0ffff0);

    // Target should be much closer to windowStartTarget than recentTarget
    // (allowing some deviation due to timing variations)
    arith_uint256 diffFromWindowStart = target > windowStartTarget ?
        target - windowStartTarget : windowStartTarget - target;
    arith_uint256 diffFromRecent = target > recentTarget ?
        target - recentTarget : recentTarget - target;

    BOOST_CHECK(diffFromWindowStart < diffFromRecent);
}

/* Test LWMAv2 3x cap enforcement */
BOOST_AUTO_TEST_CASE(lwmav2_cap_enforcement)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    Consensus::Params params = chainParams->GetConsensus();

    // Set activation heights for testing
    params.nLWMAHeight = 100;
    params.nLWMAFixHeight = 150;
    params.nLWMAWindow = 10;

    // Create blocks with extremely fast solvetimes to trigger cap
    const int numBlocks = 15;
    std::vector<CBlockIndex> blocks(numBlocks);

    for (int i = 0; i < numBlocks; i++) {
        blocks[i].pprev = i > 0 ? &blocks[i - 1] : nullptr;
        blocks[i].nHeight = params.nLWMAFixHeight + i;
        // Very fast blocks (1 second each instead of 150 seconds)
        blocks[i].nTime = 1394325760 + i * 1;
        blocks[i].nBits = 0x1e0ffff0;
    }

    CBlockHeader header;
    header.nTime = blocks[numBlocks - 1].nTime + 1;

    // Get LWMAv2 result with extreme fast blocks
    unsigned int result = GetNextWorkRequiredLWMAv2(&blocks[numBlocks - 1], &header, params);

    // Result should be valid
    BOOST_CHECK(result != 0);
    arith_uint256 target;
    bool neg, over;
    target.SetCompact(result, &neg, &over);
    BOOST_CHECK(!neg && !over);

    // With 3x cap, target should be at most 3x lower than window-start target
    arith_uint256 windowStartTarget;
    windowStartTarget.SetCompact(0x1e0ffff0);
    arith_uint256 minAllowedTarget = windowStartTarget / 3;

    BOOST_CHECK(target >= minAllowedTarget);
}

BOOST_AUTO_TEST_SUITE_END()
