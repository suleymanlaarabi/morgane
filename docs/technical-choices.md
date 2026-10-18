# Technical choices

Each choice below lists the alternatives we compared and why we kept ours.

## Language: C++23

The project requires C++. C++23 brings `std::format`, `std::span`, designated initializers,
`std::move_only_function` and better `constexpr`, which keep the ECS and network code short
without macros. GCC 14 supports all of it.

## Architecture: an entity component system (SIECS)

| | Inheritance tree (`GameObject`) | EnTT | flecs | SIECS |
| --- | --- | --- | --- | --- |
| Composition of behaviours | Rigid hierarchies | Yes | Yes | Yes |
| Scheduler with phases and declared access | No | No (do it yourself) | Yes | Yes |
| Relations (ownership, hierarchies, cleanup) | Manual | Manual | Yes | Yes |
| Observers on add/set/remove and custom events | Manual | Signals | Yes | Yes |
| C API usable from C and C++ | No | No | Yes | Yes |
| Maintained by the team | — | No | No | Yes |

flecs offers the same features but is a large codebase we would use as a black box. SIECS, our
own ECS, gives us the same model with code we understand, can debug and can extend when the game
needs it (we fixed several of its observer and command bugs while building this game). The
networking and physics layers are themselves SIECS modules, so the whole game uses one model.

## Rendering, audio and input: raylib + raygui

| | SFML | SDL2/3 | raylib |
| --- | --- | --- | --- |
| Scope | 2D graphics, audio, network | Windowing, input, audio; rendering is low level | 2D/3D drawing, audio, input in one C library |
| Build | Several modules, C++ | Needs a renderer on top | One small C library, CMake |
| Immediate-mode UI | Third-party | Third-party | raygui (same author, header only) |

raylib draws sprites and text with one call, plays generated sounds, and builds anywhere CMake
does. raygui adds buttons, sliders and text boxes, which we wrap as ECS entities. Game engines
(Unity, Godot…) are excluded by the subject.

## Physics: Box2D 3

Collisions between ships, shots and enemies use Box2D's broad phase, collision filtering
(players never hit each other) and continuous collision for fast shots. Writing our own would
mean reimplementing a broad phase and tunnelling prevention. Box2D 3 has a C API and handles
thousands of bodies; we drive it from ECS systems at a fixed 60 Hz.

## Networking: UDP with our own reliability layer

| | TCP | ENet / yojimbo | asio + custom | Raw sockets + custom (ours) |
| --- | --- | --- | --- | --- |
| Head-of-line blocking | Yes, for all data | No | Depends | No |
| Unreliable sequenced data | No | Yes | Yes | Yes |
| Integrates with the ECS change stream | — | Needs glue | Needs glue | Built in |

The subject requires UDP. Game state mixes data that must arrive (spawns, health) with data
that only matters when fresh (positions every tick). Our protocol sends both on one socket:
reliable packets are acknowledged and resent, unreliable ones are dropped when late. Sockets are
wrapped in a `Transport` class with its own thread and lock-free queues, so the game loop never
blocks. Replication reads the ECS change events directly, which no off-the-shelf library does.

## Server authority with client prediction

The server alone decides hits, deaths and spawns, so every player sees the same game and a
modified client cannot cheat on outcomes. To keep controls responsive, the client predicts its
own ship with the shared movement rule and reconciles with the server; other entities are
interpolated or extrapolated from their trajectories. This is the model of most action games
(Quake, Source, Overwatch).

## Dependencies: vcpkg

| | System packages | Conan | CMake CPM | vcpkg |
| --- | --- | --- | --- | --- |
| Reproducible versions | No | Yes | Yes | Yes (manifest + baseline) |
| Builds without extra tooling | — | Python tool | Yes | Bootstrap script |
| Custom recipes for our libraries | — | Yes | Yes | Overlay ports |

vcpkg pins every library to an exact version with a manifest and overlay ports, binary-caches
them in CI, and works from CMake presets. The bootstrap script fetches a pinned vcpkg, so a fresh
Linux machine only needs a compiler and CMake.

## Sounds generated at startup

The sound effects are synthesized in code (square sweeps, filtered noise, a short melody)
instead of loaded from files: no licensing questions, no assets to ship, and each sound is a few
lines anyone can tune.

## Documentation: MkDocs

Markdown pages in the repository, built into a navigable static site by MkDocs. Pages are
reviewed with the code they describe, and readable directly on GitHub.
