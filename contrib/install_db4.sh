#!/bin/sh
# Copyright (c) 2017-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

# Install libdb4.8 (Berkeley DB).

export LC_ALL=C
set -e

if [ -z "${1}" ]; then
  echo "Usage: $0 <base-dir> [<extra-bdb-configure-flag> ...]"
  echo
  echo "Must specify a single argument: the directory in which db4 will be built."
  echo "This is probably \`pwd\` if you're at the root of the doriancoin repository."
  exit 1
fi

expand_path() {
  cd "${1}" && pwd -P
}

BDB_PREFIX="$(expand_path ${1})/db4"; shift;

# Resolve these before anything cd's, so a caller can pass paths relative to
# their own working directory.
if [ -n "${CONFIG_GUESS_SRC}" ]; then
  CONFIG_GUESS_SRC="$(expand_path "$(dirname "${CONFIG_GUESS_SRC}")")/$(basename "${CONFIG_GUESS_SRC}")"
fi
if [ -n "${CONFIG_SUB_SRC}" ]; then
  CONFIG_SUB_SRC="$(expand_path "$(dirname "${CONFIG_SUB_SRC}")")/$(basename "${CONFIG_SUB_SRC}")"
fi
BDB_VERSION='db-4.8.30.NC'
BDB_HASH='12edc0df75bf9abd7f82f821795bcee50f42cb2e5f76a6a281b85732798364ef'
BDB_URL="https://download.oracle.com/berkeley-db/${BDB_VERSION}.tar.gz"

check_exists() {
  command -v "$1" >/dev/null
}

sha256_check() {
  # Args: <sha256_hash> <filename>
  #
  # Compute the digest and compare it rather than relying on a -c flag: GNU
  # and BSD builds of these tools disagree about whether a checklist may
  # arrive on stdin, and macOS 15 ships a BSD sha256sum that rejects it.
  if check_exists sha256sum; then
    sha256_computed="$(sha256sum "${2}" | cut -d' ' -f1)"
  elif check_exists shasum; then
    sha256_computed="$(shasum -a 256 "${2}" | cut -d' ' -f1)"
  elif check_exists sha256; then
    sha256_computed="$(sha256 -q "${2}")"
  else
    echo "no sha256 utility found" >&2
    exit 1
  fi

  if [ "${sha256_computed}" != "${1}" ]; then
    echo "${2}: FAILED" >&2
    echo "  expected ${1}" >&2
    echo "  computed ${sha256_computed}" >&2
    exit 1
  fi
  echo "${2}: OK"
}

http_get() {
  # Args: <url> <filename> <sha256_hash>
  #
  # It's acceptable that we don't require SSL here because we manually verify
  # content hashes below.
  #
  if [ -f "${2}" ]; then
    echo "File ${2} already exists; not downloading again"
  elif check_exists curl; then
    curl --insecure --location --fail --retry 5 "${1}" -o "${2}"
  else
    wget --no-check-certificate "${1}" -O "${2}"
  fi

  sha256_check "${3}" "${2}"
}

mkdir -p "${BDB_PREFIX}"
http_get "${BDB_URL}" "${BDB_VERSION}.tar.gz" "${BDB_HASH}"
tar -xzvf ${BDB_VERSION}.tar.gz -C "$BDB_PREFIX"
cd "${BDB_PREFIX}/${BDB_VERSION}/"

# Apply a patch necessary when building with clang and c++11 (see https://community.oracle.com/thread/3952592)
CLANG_CXX11_PATCH_URL='https://gist.githubusercontent.com/LnL7/5153b251fd525fe15de69b67e63a6075/raw/7778e9364679093a32dec2908656738e16b6bdcb/clang.patch'
CLANG_CXX11_PATCH_HASH='7a9a47b03fd5fb93a16ef42235fa9512db9b0829cfc3bdf90edd3ec1f44d637c'
http_get "${CLANG_CXX11_PATCH_URL}" clang.patch "${CLANG_CXX11_PATCH_HASH}"
patch -p2 < clang.patch

# The packaged config.guess and config.sub are ancient (2009) and can cause build issues.
# Replace them with modern versions.
# See https://github.com/bitcoin/bitcoin/issues/16064
CONFIG_GUESS_URL='https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.guess;hb=55eaf3e779455c4e5cc9f82efb5278be8f8f900b'
CONFIG_GUESS_HASH='2d1ff7bca773d2ec3c6217118129220fa72d8adda67c7d2bf79994b3129232c1'
CONFIG_SUB_URL='https://git.savannah.gnu.org/gitweb/?p=config.git;a=blob_plain;f=config.sub;hb=55eaf3e779455c4e5cc9f82efb5278be8f8f900b'
CONFIG_SUB_HASH='3a4befde9bcdf0fdb2763fc1bfa74e8696df94e1ad7aac8042d133c8ff1d2e32'

rm -f "dist/config.guess"
rm -f "dist/config.sub"

# Callers that already have suitable copies (the depends tree ships a pair)
# can point at them and skip the download entirely. Savannah rate-limits, so
# for automated builds this is the difference between a reproducible run and
# an intermittent one.
if [ -n "${CONFIG_GUESS_SRC}" ] && [ -n "${CONFIG_SUB_SRC}" ]; then
  echo "Using local config.guess (${CONFIG_GUESS_SRC}) and config.sub (${CONFIG_SUB_SRC})"
  cp "${CONFIG_GUESS_SRC}" dist/config.guess
  cp "${CONFIG_SUB_SRC}" dist/config.sub
  chmod +x dist/config.guess dist/config.sub
else
  http_get "${CONFIG_GUESS_URL}" dist/config.guess "${CONFIG_GUESS_HASH}"
  http_get "${CONFIG_SUB_URL}" dist/config.sub "${CONFIG_SUB_HASH}"
fi

cd build_unix/

"${BDB_PREFIX}/${BDB_VERSION}/dist/configure" \
  --enable-cxx --disable-shared --disable-replication --with-pic --prefix="${BDB_PREFIX}" \
  "${@}"

make install

echo
echo "db4 build complete."
echo
# shellcheck disable=SC2016
echo 'When compiling doriancoind, run `./configure` in the following way:'
echo
echo "  export BDB_PREFIX='${BDB_PREFIX}'"
# shellcheck disable=SC2016
echo '  ./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include" ...'
