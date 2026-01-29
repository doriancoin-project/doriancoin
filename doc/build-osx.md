# macOS Build Instructions and Notes

The commands in this guide should be executed in a Terminal application.
The built-in one is located in
```
/Applications/Utilities/Terminal.app
```

## Preparation
Install the macOS command line tools:

```shell
xcode-select --install
```

When the popup appears, click `Install`.

Then install [Homebrew](https://brew.sh).

## Dependencies
```shell
brew install automake libtool boost miniupnpc pkg-config python qt@5 libevent qrencode fmt
```

If you run into issues, check [Homebrew's troubleshooting page](https://docs.brew.sh/Troubleshooting).
See [dependencies.md](dependencies.md) for a complete overview.

If you want to build the disk image with `make deploy` (.dmg / optional), you need RSVG:
```shell
brew install librsvg
```

The wallet support requires one or both of the dependencies ([*SQLite*](#sqlite) and [*Berkeley DB*](#berkeley-db)) in the sections below.
To build Doriancoin Core without wallet, see [*Disable-wallet mode*](#disable-wallet-mode).

#### SQLite

Usually, macOS installation already has a suitable SQLite installation.
Also, the Homebrew package could be installed:

```shell
brew install sqlite
```

In that case the Homebrew package will prevail.

#### Berkeley DB

It is recommended to use Berkeley DB 4.8. If you have to build it yourself,
you can use [this](/contrib/install_db4.sh) script to install it
like so:

```shell
./contrib/install_db4.sh .
```

from the root of the repository.

Also, the Homebrew package could be installed:

```shell
brew install berkeley-db4
```

## Build Doriancoin Core

1. Clone the Doriancoin Core source code:
    ```shell
    git clone https://github.com/doriancoin-project/doriancoin
    cd doriancoin
    ```

2.  Build Doriancoin Core:

    Configure and build the headless Doriancoin Core binaries as well as the GUI (if Qt is found).

    You can disable the GUI build by passing `--without-gui` to configure.

    On Apple Silicon (M1/M2/M3) Macs, Homebrew installs to `/opt/homebrew` which
    is not in the default compiler search path. You must pass the paths explicitly.
    The `qt@5` package is also keg-only and requires its own pkg-config path:

    ```shell
    ./autogen.sh
    ./configure \
        --with-boost-libdir=/opt/homebrew/lib \
        LDFLAGS="-L/opt/homebrew/lib -L/opt/homebrew/opt/fmt/lib -L/opt/homebrew/opt/qt@5/lib" \
        CPPFLAGS="-I/opt/homebrew/include -I/opt/homebrew/opt/fmt/include -I/opt/homebrew/opt/qt@5/include" \
        PKG_CONFIG_PATH="/opt/homebrew/opt/qt@5/lib/pkgconfig"
    make -j$(sysctl -n hw.ncpu)
    ```

    On Intel Macs where Homebrew installs to `/usr/local`, the simpler form may work:
    ```shell
    ./autogen.sh
    ./configure PKG_CONFIG_PATH="/usr/local/opt/qt@5/lib/pkgconfig"
    make -j$(sysctl -n hw.ncpu)
    ```

3.  It is recommended to build and run the unit tests:
    ```shell
    make check
    ```

4.  You can also create a  `.dmg` that contains the `.app` bundle (optional):
    ```shell
    make deploy
    ```

## Disable-wallet mode
When the intention is to run only a P2P node without a wallet, Doriancoin Core may be
compiled in disable-wallet mode with:
```shell
./configure --disable-wallet
```

In this case there is no dependency on [*Berkeley DB*](#berkeley-db) and [*SQLite*](#sqlite).

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

## Running
Doriancoin Core is now available at `./src/doriancoind`

Before running, you may create an empty configuration file:
```shell
mkdir -p "/Users/${USER}/Library/Application Support/Doriancoin"

touch "/Users/${USER}/Library/Application Support/Doriancoin/doriancoin.conf"

chmod 600 "/Users/${USER}/Library/Application Support/Doriancoin/doriancoin.conf"
```

The first time you run doriancoind, it will start downloading the blockchain. This process could
take many hours, or even days on slower than average systems.

You can monitor the download process by looking at the debug.log file:
```shell
tail -f $HOME/Library/Application\ Support/Doriancoin/debug.log
```

## Other commands:
```shell
./src/doriancoind -daemon      # Starts the doriancoin daemon.
./src/doriancoin-cli --help    # Outputs a list of command-line options.
./src/doriancoin-cli help      # Outputs a list of RPC commands when the daemon is running.
```

## Notes
* Tested on macOS with Apple Silicon (ARM64) and Intel (x86_64) processors.
* Qt 5 is required for the GUI. Install `qt@5` from Homebrew (not `qt`, which is Qt 6).
* Building with downloaded Qt binaries is not officially supported. See the notes in [#7714](https://github.com/bitcoin/bitcoin/issues/7714).
