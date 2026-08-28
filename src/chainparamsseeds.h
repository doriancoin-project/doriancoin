#ifndef BITCOIN_CHAINPARAMSSEEDS_H
#define BITCOIN_CHAINPARAMSSEEDS_H
/**
 * List of fixed seed nodes for the Doriancoin network
 *
 * Each line contains a BIP155 serialized (networkID, addr, port) tuple.
 * Format: 0x01 = IPv4, 0x04 = addr length, <4 bytes IP>, <2 bytes port big-endian>
 */
static const uint8_t chainparams_seed_main[] = {
    // 99.127.49.102:1949
    0x01,0x04,0x63,0x7f,0x31,0x66,0x07,0x9d,
};

#endif // BITCOIN_CHAINPARAMSSEEDS_H