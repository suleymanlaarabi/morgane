# Morgane / R-Type

A cooperative R-Type in C++23 for one to four players. The server owns the simulation and runs
it at a fixed 60 Hz; clients send their input over UDP, predict their own ship and draw the
replicated game.

**Documentation:** <https://suleymanlaarabi.github.io/morgane/> (sources in [docs/](docs/index.md),
built with [Zensical](https://zensical.org/); locally: `zensical serve`):
[getting started](docs/getting-started.md), [playing](docs/playing.md),
[architecture](docs/architecture.md), [development guide](docs/development.md),
[technical choices](docs/technical-choices.md), [network protocol](docs/protocol.md),
[accessibility](docs/accessibility.md).

## Quick start (Linux)

Requirements: CMake 3.25+, Ninja, GCC 14+, Git, Python 3, curl, zip, unzip, tar and, for the
client, the X11/OpenGL development headers (see [getting started](docs/getting-started.md)).
Every library is built by the pinned vcpkg manifest.

```sh
./scripts/bootstrap.sh
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug
```

Then run `./build/linux-debug/bin/r-type_server [port]` and, once per player,
`./build/linux-debug/bin/r-type_client [address[:port]] [--join]`. With `make`: `make` starts a
server and a client on its menu, `make server` and `make client` start one of them, and
`make coop PLAYERS=4` starts a server and four clients that join it.

## Controls

Arrows or WASD move, Space fires (hold), Escape opens the pause menu. After a game over, Enter
votes to play again and M returns to the menu. Every key can be remapped in **Options**.

## Layout

- `server/`: authoritative game rules and `r-type_server`.
- `client/`: `r-type_client`: raylib rendering and audio (`client/raylib`), raygui UI
  (`client/gui`), scenes (`client/scene`) and the game's client modules (`client/src`).
- `shared/`: the network schema and the movement rule both sides run.
- `libs/net/`: `sinet`, UDP transport, reliability and ECS replication.
- `libs/physics/`: `sibox2d`, fixed-step SIECS/Box2D integration.
- `tests/`: the headless `r-type_bot` client and end-to-end match scenarios.
- `vcpkg-ports/`: pinned dependency recipes. `sicore`, `sireflect` and `sijson` carry a
  `cmake-support.patch` until their repositories ship CMake support; `siecs` builds from its
  pinned commit as is. `box2d` (3.2 dev), `raylib` (6.2) and `raygui` (5.1 dev) are newer than
  the vcpkg baseline.

Configuring a preset links its `compile_commands.json` at the repository root for clangd.
`.zed/` holds editor settings and tasks.

## Authors

- Suleyman Laarabi: <suleyman.laarabi@epitech.eu>
- Inès Sanchez: <ines.sanchez@epitech.eu>

Bugs and questions: [GitHub issues](https://github.com/suleymanlaarabi/morgane/issues).

## License

[MIT](LICENSE). The sprites in `assets/` come from the original R-Type and are only used for
this educational project; the sound effects are synthesized by the game.
