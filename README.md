# Morgane / R-Type

A cooperative R-Type prototype in C++23. The server owns the simulation; clients
send input over UDP and render replicated entities with local movement prediction.

## Requirements

Linux x86-64, CMake 3.25+, Ninja, GCC 14+, Git, Python 3, curl, zip, unzip and tar.
The client also needs the X11/OpenGL development headers. On Ubuntu 24.04:

```sh
sudo apt-get install build-essential gcc-14 g++-14 cmake ninja-build git python3 curl zip unzip tar \
    libgl1-mesa-dev libx11-dev libxcursor-dev libxinerama-dev libxrandr-dev libxi-dev
export CC=gcc-14 CXX=g++-14
```

SIECS, Box2D, raylib and Catch2 are built by vcpkg; nothing else is installed globally.

## Build, test and run

```sh
./scripts/bootstrap.sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Then, in two terminals, run `./build/linux-debug/bin/r-type_server` and
`./build/linux-debug/bin/r-type_client`. The client connects to `127.0.0.1:4242`.
Enter starts the game, arrows/WASD move, Escape pauses and M returns to the menu.

Use `linux-release` for an optimized build. Add `-DMORGANE_BUILD_CLIENT=OFF` to
configure a headless server without raylib. `cmake --install build/linux-release
--prefix package` produces a self-contained application directory.

## Tests

Unit tests use [Catch2](https://github.com/catchorg/Catch2) and live next to the
code they test (`*_test.cpp`). Each module builds its own test executable, and
`ctest` discovers every test case. To add a test, write a `TEST_CASE` in the
module's test file, or add a new `*_test.cpp` to that module's test target.

## Layout

- `libs/net/`: UDP transport, protocol and replication (`sinet`).
- `libs/physics/`: fixed-step SIECS/Box2D integration (`sibox2d`).
- `server/`: authoritative simulation and server executable.
- `client/`: rendering, input, animation, scenes and client executable.
- `shared/`: gameplay schema and shared movement rules.
- `vcpkg-ports/`: pinned dependency recipes. The `sicore`, `sireflect`, `sijson`
  and `siecs` ports carry a `cmake-support.patch` until their repositories ship
  CMake support; then pin the new commit and delete the patch. `box2d` (3.2 dev)
  and `raylib` (6.2) are newer than the vcpkg baseline.

Configuring a preset links its `compile_commands.json` at the repository root for
clangd. `.zed/` holds editor settings and tasks.
