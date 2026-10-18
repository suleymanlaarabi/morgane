# Getting started

## Requirements

Linux x86-64 with CMake 3.25+, Ninja, GCC 14+, Git, Python 3, curl, zip, unzip and tar. The client
also needs the X11/OpenGL development headers. On Ubuntu 24.04:

```sh
sudo apt-get install build-essential gcc-14 g++-14 cmake ninja-build git python3 curl zip unzip tar \
    libgl1-mesa-dev libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev
export CC=gcc-14 CXX=g++-14
```

Every library (SIECS, Box2D, raylib, raygui, Catch2) is built by the pinned vcpkg manifest, so
nothing else is installed globally and every machine builds the same versions.

## Build and test

```sh
./scripts/bootstrap.sh          # fetch the pinned vcpkg tool
cmake --preset linux-debug      # configure; vcpkg builds the dependencies the first time
cmake --build --preset linux-debug
ctest --preset linux-debug      # unit tests and end-to-end match scenarios
```

`make` wraps these steps: `make build` and `make test`. To play, `make` alone builds, then starts
a server and a client on its menu; `make server` and `make client` start just one of them, and
`make coop PLAYERS=4` starts a server and four clients that join it at once. `PORT` and `ADDRESS`
change where the server listens and where clients connect, e.g.
`make client ADDRESS=192.168.1.20:4242`.

Other presets: `linux-release` for optimized binaries. Add `-DMORGANE_BUILD_CLIENT=OFF` to build a
headless server without raylib. `cmake --install build/linux-release --prefix package` produces a
self-contained directory with both executables and the assets.

## Run

```sh
./build/linux-debug/bin/r-type_server [port]                 # UDP port 4242 by default
./build/linux-debug/bin/r-type_client [address[:port]] [--join]
```

The client opens on its menu with the address field filled in (`127.0.0.1:4242` by default).
Press Enter or click **Join** to join. `--join` skips the menu and connects at once.

Up to four clients can join one server. Start more clients on other machines with the server's
address, for example `r-type_client 192.168.1.20:4242`. The server must be reachable on its UDP
port.

## Tests

| Suite | What it checks |
| --- | --- |
| `net_tests` | Packet headers, frames, acknowledgements and the receive window |
| `physics_tests` | Fixed stepping, body lifecycle and contacts |
| `server_tests` | Gameplay rules: slots, movement, firing, damage, waves, match phases, restart votes, departures |
| `gui_tests` | UI layout, headless |
| `match crowd` | Four bots join, play and destroy enemies; a fifth is refused because the match is full |
| `match crash` | A bot is killed abruptly; the others are told and keep playing; its slot can be joined again |
| `match restart` | Two bots lose, vote, and the match restarts with full health |

The `match` scenarios (`tests/match_test.py`) start a real server and the headless `r-type_bot`
client on a free localhost port. Run only them with `ctest --preset linux-debug -L network`.
