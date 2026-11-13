#!/usr/bin/env python3
# Copyright (c) 2026 The Doriancoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Preserve pegout maturity when a restart and reorg restore a spent output."""

from decimal import Decimal

from test_framework.dsv_util import FIRST_MWEB_HEIGHT, get_hogex_tx, setup_mweb_chain
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class MWEBPegoutUndoTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1
        self.extra_args = [["-mwebheight={}".format(FIRST_MWEB_HEIGHT), "-persistmempool=0"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def assert_immature(self, raw_spend, spend_txid):
        node = self.nodes[0]
        assert spend_txid not in node.getrawmempool()
        result = node.testmempoolaccept([raw_spend])[0]
        assert_equal(result['allowed'], False)
        assert_equal(result['reject-reason'], 'bad-txns-premature-spend-of-pegout')

    def run_test(self):
        node = self.nodes[0]
        setup_mweb_chain(node)

        # A separately funded wallet has only MWEB coins, ensuring this send
        # creates a pegout rather than selecting the miner's ordinary UTXOs.
        node.createwallet('pegger')
        miner = node.get_wallet_rpc(self.default_wallet_name)
        pegger = node.get_wallet_rpc('pegger')
        miner.sendtoaddress(pegger.getnewaddress(address_type='mweb'), 2)
        node.generate(1)
        destination = miner.getnewaddress(address_type='bech32')
        pegger.sendtoaddress(destination, 1)
        pegout_block = node.generate(1)[0]
        pegout_height = node.getblockcount()
        hogex = get_hogex_tx(node, pegout_block)
        script = bytes.fromhex(miner.getaddressinfo(destination)['scriptPubKey'])
        indexes = [i for i, output in enumerate(hogex.vout) if output.scriptPubKey == script]
        assert_equal(len(indexes), 1)
        pegout_vout = indexes[0]
        assert pegout_vout > 0
        raw_spend = node.createrawtransaction(
            [{'txid': hogex.hash, 'vout': pegout_vout}],
            {miner.getnewaddress(): Decimal('0.999')},
            0,  # locktime: maturity must be the only reason for rejecting the spend.
        )
        signed = miner.signrawtransactionwithwallet(raw_spend)
        assert_equal(signed['complete'], True)
        raw_spend = signed['hex']
        spend_txid = node.decoderawtransaction(raw_spend)['txid']
        self.assert_immature(raw_spend, spend_txid)

        self.log.info('Mature and spend the exact HogEx pegout, then restart')
        maturity_blocks = node.generate(5)
        assert_equal(node.sendrawtransaction(raw_spend), spend_txid)
        node.generate(1)
        assert_equal(node.gettxout(hogex.hash, pegout_vout, False), None)
        self.restart_node(0)

        self.log.info('Disconnect the spend back to the pegout height using disk undo')
        node.invalidateblock(maturity_blocks[0])
        assert_equal(node.getblockcount(), pegout_height)
        restored = node.gettxout(hogex.hash, pegout_vout, False)
        assert_equal(restored['confirmations'], 1)
        assert_equal(restored['value'], Decimal('1'))
        self.assert_immature(raw_spend, spend_txid)

        self.log.info('Restored pegout classification also survives a second restart')
        self.restart_node(0)
        self.assert_immature(raw_spend, spend_txid)
        node.generate(4)
        self.assert_immature(raw_spend, spend_txid)
        node.generate(1)
        assert_equal(node.testmempoolaccept([raw_spend])[0]['allowed'], True)
        assert_equal(node.sendrawtransaction(raw_spend), spend_txid)
        node.generate(1)
        assert_equal(node.gettxout(hogex.hash, pegout_vout, False), None)


if __name__ == '__main__':
    MWEBPegoutUndoTest().main()
