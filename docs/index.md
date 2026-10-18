# Morgane R-Type

Morgane is a cooperative remake of the arcade shoot'em up R-Type. One to four players join a
server, fly their ships, shoot waves of enemies, lose together and start again together.

The server owns the game: it runs the simulation at a fixed 60 Hz and sends the result to the
clients over UDP. Clients send their input, draw what the server says, and predict their own ship
so that it answers the keys at once.

| You want to | Read |
| --- | --- |
| Build, run and test the game | [Getting started](getting-started.md) |
| Learn the controls and the rules | [Playing](playing.md) |
| Understand how the code is organised | [Architecture](architecture.md) |
| Add a feature or a test | [Development guide](development.md) |
| Know why these libraries and designs | [Technical choices](technical-choices.md) |
| Write another client for the server | [Network protocol](protocol.md) |
| Know how the game serves players with disabilities | [Accessibility](accessibility.md) |

## Repository at a glance

| Path | Content |
| --- | --- |
| `server/` | Authoritative game rules and the `r-type_server` executable |
| `client/` | The `r-type_client` executable: rendering, audio, input, UI, scenes |
| `shared/` | The network schema and the movement rule both sides run |
| `libs/net/` | `sinet`: UDP transport, reliability and ECS replication |
| `libs/physics/` | `sibox2d`: fixed-step Box2D physics driven by ECS systems |
| `tests/` | End-to-end match scenarios with a headless bot client |
| `docs/` | This documentation, built with [MkDocs](https://www.mkdocs.org/) |

To browse these pages locally: `pip install mkdocs`, then `mkdocs serve` from the repository root.
