# Development guide

## Conventions

- C++23, formatted with the repository's `.clang-format` (LLVM style, four spaces, 100 columns).
- `snake_case` functions, fields and files; `PascalCase` components, resources and modules;
  lowercase namespaces (`game`, `net`, `ray`, `gui`, `physics`).
- Data goes in components and resources, behaviour in systems and observers, features in
  modules. Use SIECS queries, relations, requirements (`with<>`) and scheduling instead of custom
  registries or callback lists.
- Write fields in place through `&` system parameters; use `set()` when observers must hear about
  the change, which includes every replicated component.

## Adding a gameplay feature

1. Put the server-side data and rules in a module under `server/src/`, imported by
   `ServerGameplay` (`server/src/gameplay.h`). Systems of the simulation run in the physics
   phases: `ecs::resource<physics::Phases>().input` or `.output`, with `.manual()`.
2. If clients must see it, add a reflected component or event to `shared/game.h` and register it
   in `register_network()`: `net::replicate<T>()` for state, `net::event<T>()` for one-off
   messages (`.server_to_clients()` for server events), `.reliable()` unless a newer value
   always replaces an older one.
3. Display it on the client in a module under `client/src/`, imported in `client/src/main.cpp`.
4. Test the rule in `server/src/gameplay_test.cpp`, and the network path with a scenario in
   `tests/match_test.py` when it involves connections.

Changing the network schema changes the hashes clients and servers compare at connection time.
Run `r-type_bot --schema` and update the table in the [protocol](protocol.md).

## Tests

```sh
ctest --preset linux-debug                 # everything
ctest --preset linux-debug -L network      # end-to-end scenarios only
./build/linux-debug/bin/server_tests "[tag or test name]"
```

Unit tests use [Catch2](https://github.com/catchorg/Catch2) and live next to the code
(`*_test.cpp`). The gameplay fixture starts a fresh world per test without any network, and
steps the simulation tick by tick with `physics::advance`.

`tests/bot.cpp` is a headless client: `r-type_bot address port seconds [--fire] [--vote]` joins,
wanders, optionally fires and votes to play again, and prints one line per event it observes.
`tests/match_test.py` drives a server and several bots and checks those lines.

The graphical client cannot be checked headless: for rendering or UI changes, run the client and
go through the menu, a match, the pause screen and the options.

## Dependencies

`vcpkg.json` lists the libraries; `vcpkg-ports/` pins the ones newer than the vcpkg baseline or
not in its registry (SIECS and its companions, Box2D 3.2, raylib 6, raygui). To update SIECS,
point `REF` in `vcpkg-ports/siecs/portfile.cmake` at the new commit, update its `SHA512` (the
hash of the GitHub archive) and `version-string`, then reconfigure.
