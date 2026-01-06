# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Doriancoin Core is a cryptocurrency/blockchain implementation forked from Bitcoin Core. It's a peer-to-peer digital currency with MWEB (MimbleWimble Extension Block) support. The codebase is primarily C++ with Python functional tests.

## Build Commands

```bash
# Generate build system (required first time or after configure.ac changes)
./autogen.sh

# Configure (basic)
./configure

# Configure with debugging symbols
./configure --enable-debug

# Configure with sanitizers
./configure --with-sanitizers=address,undefined

# Build
make

# Build with parallelism
make -j$(nproc)

# Install (optional)
make install
```

## Testing Commands

```bash
# Run unit tests
make check

# Run a single unit test suite
src/test/test_doriancoin --run_test=<suite_name>

# Run all functional tests
test/functional/test_runner.py

# Run a single functional test
test/functional/feature_rbf.py
# or
test/functional/test_runner.py feature_rbf.py

# Run functional tests with parallelism
test/functional/test_runner.py --jobs=8

# Run extended tests
test/functional/test_runner.py --extended

# Run linting
test/lint/lint-all.sh

# Run with Valgrind
valgrind --suppressions=contrib/valgrind.supp src/test/test_doriancoin
```

## Build Outputs

- `doriancoind` - Headless daemon
- `doriancoin-qt` - GUI wallet application
- `doriancoin-cli` - Command-line RPC client
- `doriancoin-tx` - Transaction utility
- `doriancoin-wallet` - Offline wallet tool

## Architecture Overview

### Core Libraries (src/)
- `libbitcoin_server` - Node/server functionality
- `libbitcoin_wallet` - Wallet functionality (when ENABLE_WALLET)
- `libbitcoin_common` - Shared code between server and client
- `libbitcoin_consensus` - Consensus-critical code
- `libbitcoin_util` - Utility functions
- `libbitcoin_crypto` - Cryptographic primitives (with optional SSE41/AVX2/SHANI)
- `libmw` - MimbleWimble Extension Block implementation

### Key Source Directories
- `src/consensus/` - Consensus rules (changes here require extreme care)
- `src/validation.cpp` - Block and transaction validation
- `src/net_processing.cpp` - P2P message handling
- `src/script/` - Bitcoin Script interpreter
- `src/wallet/` - Wallet implementation
- `src/rpc/` - RPC interface implementations
- `src/mweb/` - MWEB integration code
- `src/libmw/` - MimbleWimble library
- `src/qt/` - Qt GUI code

### Test Structure
- `src/test/` - C++ unit tests (Boost.Test framework)
- `test/functional/` - Python functional/integration tests
- `test/lint/` - Static analysis scripts
- `test/util/` - Utility tests

## Code Style

### C++
- 4-space indentation, no tabs
- Braces on new lines for classes/functions, same line for control flow
- `snake_case` for variables, `PascalCase` for classes/functions
- Member variables: `m_` prefix, globals: `g_` prefix
- Use `nullptr` not `NULL`, prefer `++i` over `i++`
- Run `contrib/devtools/clang-format-diff.py` before submitting

### Python (functional tests)
- Follow PEP-8 guidelines
- Use `'{}'.format(x)` not `'%s' % x`
- Use type hints
- Test naming: `<area>_test.py` (feature_, wallet_, rpc_, p2p_, etc.)

## Key Development Notes

- New features should be exposed via RPC first, then GUI
- Never use `std::map[]` for reading (use `.find()`) - `[]` inserts default on miss
- Initialize all non-static class members at declaration
- Use `ParseInt32/64`, `ParseUInt32/64` for number parsing (locale-safe)
- Avoid locale-dependent functions
- Run with `-regtest` for local testing, `-testnet` for network testing
- Debug logs go to `debug.log` in data directory; use `-debug=<category>`
- `--enable-debug` adds `DEBUG_LOCKORDER` for deadlock detection

## PR Conventions

Prefix PR titles with component area:
- `consensus:` - Consensus-critical changes
- `wallet:` - Wallet changes
- `qt:` / `gui:` - GUI changes
- `rpc:` / `rest:` / `zmq:` - API changes
- `net:` / `p2p:` - Networking
- `test:` / `qa:` - Testing
- `build:` - Build system
- `doc:` - Documentation
