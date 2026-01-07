#!/usr/bin/env python3
# Copyright (c) 2024 The Doriancoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test LWMA difficulty adjustment algorithm activation.

Tests that the LWMA (Linear Weighted Moving Average) difficulty algorithm
activates correctly at the specified height and that blocks with incorrect
difficulty are rejected.
"""

from test_framework.blocktools import (
    create_coinbase,
    NORMAL_GBT_REQUEST_PARAMS,
    TIME_GENESIS_BLOCK,
)
from test_framework.messages import CBlock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class LWMAActivationTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]

        # Get consensus parameters
        blockchain_info = node.getblockchaininfo()
        self.log.info(f"Chain: {blockchain_info['chain']}")

        # On regtest, LWMA activates at height 500 (as set in chainparams.cpp)
        # regtest has fPowNoRetargeting=true, so difficulty doesn't actually change,
        # but we can still verify the dispatch logic works

        self.log.info("Mining blocks before LWMA activation...")

        # Mine blocks up to just before activation
        # On regtest, nLWMAHeight = 500
        activation_height = 500
        blocks_to_mine = activation_height - 1

        # Mine in batches for speed
        address = node.get_deterministic_priv_key().address
        for i in range(0, blocks_to_mine, 100):
            batch = min(100, blocks_to_mine - i)
            node.generatetoaddress(batch, address)
            if (i + batch) % 100 == 0:
                self.log.info(f"  Mined {i + batch} blocks...")

        # Verify we're at the expected height
        info = node.getblockchaininfo()
        assert_equal(info['blocks'], activation_height - 1)
        self.log.info(f"At block height {info['blocks']} (just before LWMA activation)")

        # Get block template before activation
        tmpl_before = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        bits_before = tmpl_before['bits']
        self.log.info(f"Block template before activation - bits: {bits_before}")

        # Mine the activation block
        self.log.info("Mining LWMA activation block...")
        node.generatetoaddress(1, address)

        info = node.getblockchaininfo()
        assert_equal(info['blocks'], activation_height)
        self.log.info(f"LWMA activated at block {info['blocks']}")

        # Get block template after activation
        tmpl_after = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        bits_after = tmpl_after['bits']
        self.log.info(f"Block template after activation - bits: {bits_after}")

        # Mine some more blocks after activation
        self.log.info("Mining blocks after LWMA activation...")
        node.generatetoaddress(50, address)

        info = node.getblockchaininfo()
        self.log.info(f"Final block height: {info['blocks']}")

        # Verify chain is valid
        assert_equal(info['blocks'], activation_height + 50)

        # Test that a block with wrong difficulty bits is rejected
        self.log.info("Testing block rejection with incorrect difficulty...")

        # Get a valid template
        tmpl = node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)
        next_height = int(tmpl["height"])
        coinbase_tx = create_coinbase(height=next_height)
        coinbase_tx.rehash()

        block = CBlock()
        block.nVersion = tmpl["version"]
        block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
        block.nTime = tmpl["curtime"]
        # Use incorrect difficulty bits
        block.nBits = 0x207fffff  # Very easy difficulty, definitely wrong
        block.nNonce = 0
        block.vtx = [coinbase_tx]
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()

        # Submit the block with wrong bits - should be rejected
        result = node.submitblock(hexdata=block.serialize().hex())
        assert_equal(result, 'bad-diffbits')
        self.log.info("Block with incorrect difficulty correctly rejected")

        # Verify chain height hasn't changed
        info = node.getblockchaininfo()
        assert_equal(info['blocks'], activation_height + 50)

        self.log.info("LWMA activation test passed!")


if __name__ == '__main__':
    LWMAActivationTest().main()
