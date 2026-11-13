#!/usr/bin/env python3
# Copyright (c) 2026 The Doriancoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reject empty kernel feature vectors even when nonstandard txs are accepted."""

import hashlib

from test_framework.blocktools import NORMAL_GBT_REQUEST_PARAMS
from test_framework.dsv_util import FIRST_MWEB_HEIGHT, setup_mweb_chain
from test_framework.key import (
    SECP256K1,
    SECP256K1_FIELD_SIZE,
    SECP256K1_G,
    SECP256K1_ORDER,
)
from test_framework.messages import CBlock, CTransaction, FromHex, Hash, MWEBKernel, blake3, ser_string
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error


def with_signed_kernel(tx_hex: str, features: int, extradata: bytes = b'') -> str:
    """Append a zero-supply kernel, preserving all signatures and balance sums."""
    blind = 7
    pubkey = SECP256K1.affine(SECP256K1.mul([(SECP256K1_G, blind)]))
    x_bytes = pubkey[0].to_bytes(32, 'big')
    is_quad = pow(pubkey[1], (SECP256K1_FIELD_SIZE - 1) // 2, SECP256K1_FIELD_SIZE) == 1
    commitment = bytes([9 ^ is_quad]) + x_bytes
    compressed_pubkey = bytes([2 | (pubkey[1] & 1)]) + x_bytes

    # libmw omits empty vectors from the signature message, even when their
    # feature bits are set. The wire encoding below still needs their lengths.
    message = blake3(bytes([features]) + commitment + (ser_string(extradata) if extradata else b'')).to_byte_arr()

    # MWEB uses the legacy Schnorr scheme in secp256k1-zkp, not BIP340:
    # R has quadratic-residue y; e = SHA256(R.x || compressed P || message).
    nonce = int.from_bytes(hashlib.sha256(blind.to_bytes(32, 'big') + message).digest(), 'big') % SECP256K1_ORDER
    assert nonce != 0
    nonce_point = SECP256K1.affine(SECP256K1.mul([(SECP256K1_G, nonce)]))
    if pow(nonce_point[1], (SECP256K1_FIELD_SIZE - 1) // 2, SECP256K1_FIELD_SIZE) != 1:
        nonce = SECP256K1_ORDER - nonce
    r_bytes = nonce_point[0].to_bytes(32, 'big')
    challenge = int.from_bytes(hashlib.sha256(r_bytes + compressed_pubkey + message).digest(), 'big') % SECP256K1_ORDER
    signature = r_bytes + ((nonce + challenge * blind) % SECP256K1_ORDER).to_bytes(32, 'big')

    kernel = MWEBKernel()
    kernel.features = features
    kernel.pegouts = []
    kernel.extradata = extradata
    kernel.excess = int.from_bytes(commitment, 'little')
    kernel.signature = int.from_bytes(signature, 'little')

    tx = FromHex(CTransaction(), tx_hex)
    assert tx.mweb_tx is not None
    assert_equal(tx.vin, [])
    assert_equal(tx.vout, [])
    assert_equal(len(tx.mweb_tx.body.kernels), 1)
    original_kernel = tx.mweb_tx.body.kernels[0]
    assert original_kernel.fee > 0 and not original_kernel.pegin and not original_kernel.pegouts

    # KernelSort orders descending by supply change: zero precedes -fee.
    tx.mweb_tx.body.kernels.insert(0, kernel)
    old_offset = int.from_bytes(tx.mweb_tx.kernel_offset.to_byte_arr(), 'big')
    new_offset = (old_offset - blind) % SECP256K1_ORDER
    tx.mweb_tx.kernel_offset = Hash.from_byte_arr(new_offset.to_bytes(32, 'big'))
    return tx.serialize().hex()


class MWEBNoncanonicalFeaturesTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [
            ['-acceptnonstdtxn=1', '-mwebheight={}'.format(FIRST_MWEB_HEIGHT)],
            ['-acceptnonstdtxn=1', '-mwebheight={}'.format(FIRST_MWEB_HEIGHT), '-walletbroadcast=0'],
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node, sender = self.nodes
        setup_mweb_chain(node)
        self.sync_all()
        node.sendtoaddress(sender.getnewaddress(address_type='mweb'), 0.9)
        node.generate(1)
        self.sync_all()

        txid = sender.sendtoaddress(node.getnewaddress(address_type='mweb'), 0.3)
        tx_hex = sender.gettransaction(txid)['hex']
        assert_equal(node.getrawmempool(), [])

        self.log.info('Accept a signed nonempty extra-data kernel under nonstandard policy')
        canonical_hex = with_signed_kernel(tx_hex, 32, b'\x01')
        # The wallet pays exactly the fixed MWEB fee for the original weight.
        # Credit the extra kernel's weight without changing signed amounts.
        node.prioritisetransaction(node.decoderawtransaction(canonical_hex)['txid'], 0, 10000)
        canonical_result = node.testmempoolaccept([canonical_hex])[0]
        assert canonical_result['allowed'], canonical_result

        for feature, reason in [(32, 'bad-mweb-empty-extradata'), (4, 'bad-mweb-empty-pegout')]:
            self.log.info('Reject a signed empty feature vector: {}'.format(reason))
            noncanonical_hex = with_signed_kernel(tx_hex, feature)
            node.prioritisetransaction(node.decoderawtransaction(noncanonical_hex)['txid'], 0, 10000)
            result = node.testmempoolaccept([noncanonical_hex])[0]
            assert not result['allowed'], result
            assert_equal(result['reject-reason'], reason)
            assert_raises_rpc_error(-26, reason, node.sendrawtransaction, noncanonical_hex)
            assert_equal(node.getrawmempool(), [])
            assert_equal(node.getblocktemplate(NORMAL_GBT_REQUEST_PARAMS)['height'], node.getblockcount() + 1)
            node.generate(1)

        self.log.info('Mine the canonical extra-data control after rejecting both empty vectors')
        accepted_txid = node.sendrawtransaction(canonical_hex)
        assert_equal(node.getrawmempool(), [accepted_txid])
        block_hash = node.generate(1)[0]
        assert_equal(node.getrawmempool(), [])
        block = FromHex(CBlock(), node.getblock(block_hash, 0))
        assert any(kernel.features == 32 and kernel.extradata == [1] for kernel in block.mweb_block.body.kernels)


if __name__ == '__main__':
    MWEBNoncanonicalFeaturesTest().main()
