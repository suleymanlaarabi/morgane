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

## Algorithms and data structures

### Existing designs we reuse

| Problem | Choice | Alternatives considered | Why |
| --- | --- | --- | --- |
| Game object storage | Archetype tables (SIECS): one contiguous column per component, one table per component set | Sparse sets (EnTT), objects in a `std::vector<GameObject*>` | Systems iterate several components at once (position, body, health); archetypes keep them contiguous and queries are cached. Adding or removing a component moves the entity to another table, which is rare in our game. |
| Entity identity | Index + generation handles, recycled through a free list (in SIECS and in replication ids) | Raw pointers, ever-growing ids | A stale id from a destroyed entity, or from an old packet, never resolves to the new entity that reuses its slot. |
| System ordering | Systems declare what they read and write; the scheduler orders them in phases | Manual call order in an `update()` function | Adding a module does not require editing a central loop. |
| Time | Fixed 60 Hz step with an accumulator, on both sides | Variable `delta_time` | The simulation is deterministic and identical on server and client, which prediction and tests rely on. |
| Collisions | Box2D's dynamic AABB tree (broad phase) and continuous collision | Brute-force pairs, uniform grid | Scales with hundreds of shots and keeps fast shots from tunnelling through enemies. |
| Thread hand-off | Two single-producer / single-consumer lock-free ring buffers (4096 packets, power of two) between the transport thread and the game loop | `std::mutex` + `std::queue` | Exactly one producer and one consumer per direction; neither side ever blocks or allocates. |
| Acknowledgements | Sequence number + latest ack + 64-bit ack bitfield in every header | One ack packet per packet, TCP-like cumulative ack | One header acknowledges the last 65 packets, so a lost ack is covered by the next one. |
| Latency hiding | Client prediction with reconciliation, snapshot interpolation 100 ms in the past, extrapolation of straight trajectories | Waiting for the server (lag), dead reckoning for everything | Standard techniques of action games; each entity kind gets the cheapest correct one. |
| Randomness | `std::mt19937` seeded from the gameplay config | `rand()`, `std::random_device` every time | Waves are reproducible, so gameplay tests are deterministic. |

### What we designed ourselves

- **Reliability layer** (`libs/net/src/protocol.h`): a sliding window of 64 reliable packets
  per peer, stored in fixed arrays, with retransmission timed from a measured round-trip time
  and a reorder buffer that delivers reliable packets in order. Unreliable packets share the
  same socket and sequence numbers but are never resent. Off-the-shelf libraries (ENet,
  yojimbo) do the same, but would not plug into the ECS change stream.
- **ECS replication** (`libs/net/src/replication.cpp`): the server listens to SIECS add/set/remove
  events instead of serializing the whole world every tick. Each peer has an outbox, a `visible`
  table (the entity ids it knows) and a byte budget per tick; a client that cannot keep up is
  disconnected with `congestion` instead of making the server's memory grow.
- **Trajectories:** shots and enemies move in straight lines, so their start point, velocity and
  start tick are sent once and both sides compute positions from them. This removes most
  per-tick traffic.

Enemy aiming is a linear scan over at most four players; nothing smarter is needed.

## Storage

| | Plain text file (ours) | JSON (sijson) | SQLite | Server-side database |
| --- | --- | --- | --- | --- |
| Data volume | A few lines | A few lines | Any | Any |
| Human-editable | Yes | Yes | No | No |
| Extra dependency | None | sijson | sqlite3 | A database server |
| Concurrent writers | No | No | Yes | Yes |
| Fits | Local settings | Structured local settings | Score boards, saves | Accounts, matchmaking |

**What is stored.** The server keeps no persistent state: a match lives in memory and ends when
its players leave, and there are no accounts or scores. The only persistent data is the client's
preferences (volume, accessibility options, key bindings), written to
`$XDG_CONFIG_HOME/morgane/preferences.txt` (or `~/.config/morgane/preferences.txt`) as
`key value` lines.

**Why a text file.** The data is a dozen values for one user on one machine: a database would add
a dependency for nothing, and a text file can be read and fixed by hand.

**Reliability.** Loading never trusts the file: unknown keys are skipped, and a value that does
not parse or is out of range (volume outside 0–1, key code outside raylib's range) keeps its
default. A missing, truncated or hand-edited file therefore still starts the game. The file is
rewritten in place, so a crash during a save can lose the newest settings; writing to a
temporary file and renaming it would make the save atomic if more data is ever stored.

**Later needs.** Score boards or saved progress would go to SQLite on the server, which handles
concurrent writes and crash recovery; accounts would need a real database server.

## Security

### Threats and mitigations per technology

| Technology | Main risks | What we do |
| --- | --- | --- |
| UDP and our binary protocol | Malformed or truncated packets, oversized lengths, forged packets, flooding, a client acting on another player's entity | Every packet is checked before use: magic number, protocol version, header size, payload size against the datagram size, and each frame's opcode and length (`valid_frames`). Buffers have fixed sizes (1200-byte MTU, 256-byte values), so a length field never drives an allocation. Entity ids are bounds- and generation-checked. Packets from unknown endpoints are ignored unless they are a valid `hello`. A random 64-bit session token ties packets to a connection. Reliable packets too far ahead of the window are dropped. The number of clients is capped. Malformed packets are covered by unit tests (`libs/net/src/protocol_test.cpp`). |
| Game logic | Cheating clients (speed hacks, fake hits) | The server is authoritative: it accepts inputs only for input types and only on the sender's own entity, clamps movement, and decides every hit, death and spawn. |
| C++ | Out-of-bounds reads and writes, use-after-free | Wire data is read through a bounds-checked `Reader` that turns any overrun into an invalid message; `std::span` and `std::array` instead of raw pointer arithmetic; generation-checked handles instead of pointers to entities. |
| raylib, raygui, Box2D | Vulnerabilities in third-party code (image decoders, parsers) | raylib only loads the assets shipped with the game, never data from the network; Box2D only receives data the server created. Every library is pinned to an exact version. |
| Build and CI | A dependency changing under us | The vcpkg baseline and overlay ports (with SHA512 checksums) pin every source archive; CI builds from those pins only. |

**Known limits.** The protocol is neither encrypted nor authenticated: someone who can read the
traffic can see the session token and forge packets for that session. There is no per-address
rate limit yet; a flood is bounded by the transport's fixed queues (excess packets are counted
and dropped), not prevented. Both are acceptable for LAN play and are the first things to add
(DTLS or a key exchange, and rate limiting) before exposing a server on the Internet.

### Long-term monitoring

- **Dependencies:** versions only change through a pull request that edits `vcpkg.json` or a
  port, and that pull request runs the full build and test suite. Before a bump we read the
  release notes and GitHub security advisories of raylib, raygui and Box2D.
  [Dependabot](https://docs.github.com/en/code-security/dependabot) (`.github/dependabot.yml`)
  opens pull requests when the GitHub Actions used by CI are updated.
- **Regression tests:** the malformed-packet and end-to-end network tests run on every push, so
  a change that weakens validation fails CI.
- **Runtime:** `sinet` counts, per peer, bytes and packets sent and received, retransmissions and
  dropped packets (`net::Stats`), and the transport counts packets dropped because its queue was
  full. A sudden rise of dropped or rejected packets is the signal to investigate.

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

## Documentation: Zensical

Markdown pages in the repository, built into a navigable static site by Zensical (from the team
behind Material for MkDocs), with navigation, an outline, a search bar and a light/dark theme,
and published to GitHub Pages by CI on every push to `main`. Pages are reviewed with the code
they describe, and readable directly on GitHub.
