# MWEB Weights & Sizes

Adapted from [Litecoin Core's MWEB documentation](https://github.com/litecoin-project/litecoin/commit/b4d7e70afc7d50b97abf70e5cf6ea401ee8ad08e) for Doriancoin.

The constants below are defined in `src/libmw/include/mw/consensus/Params.h`. `MAX_BLOCK_WEIGHT` and `MAX_NUM_INPUTS` are consensus limits. `MAX_MINE_WEIGHT` is the local mining policy limit; it does not reduce the consensus block limit. `MAX_BLOCK_BYTES` is the implementation's calculated MWEB size bound.

Additional bytes in each input/output/kernel extra-data vector, and in each pegout script, cost `ceil(bytes / 42)` weight. Each vector or script is rounded separately. See `src/libmw/include/mw/consensus/Weight.h`.

### Parameters

* MAX_BLOCK_WEIGHT = 200,000
* MAX_MINE_WEIGHT = 20,000
* BASE_OUTPUT_WEIGHT = 17
* STANDARD_OUTPUT_FIELDS_WEIGHT = 1
* STANDARD_OUTPUT_WEIGHT = 18
* BASE_KERNEL_WEIGHT = 2
* STEALTH_EXCESS_WEIGHT = 1
* KERNEL_WITH_STEALTH_WEIGHT = 3
* BYTES_PER_WEIGHT = 42
* INPUT_BYTES = 196
* MAX_NUM_INPUTS = 50,000
* MAX_BLOCK_BYTES = 21,800,195
