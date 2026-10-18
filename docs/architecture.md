# Architecture

## Overview

```
 r-type_client (x4)                                   r-type_server
 +------------------------------+     UDP      +------------------------------+
 | input -> PlayerInput --------+------------->| ServerMovement, ServerPlayers|
 | prediction (Kinematics)      |              | ServerCombat, ServerWaves    |
 | replicated entities  <-------+--------------+ ServerMatch                  |
 | rendering, audio, UI, scenes |  snapshots,  | physics (fixed 60 Hz step)   |
 +------------------------------+  events      +------------------------------+
        |            |                                  |
     raylib       sinet  <--------------------------> sinet        sibox2d (Box2D)
```

Both executables are [SIECS](https://github.com/suleymanlaarabi/siecs) applications. SIECS is an
entity component system (ECS): the state is data in **components** attached to **entities**
(or in world-wide **resources**), and the behaviour lives in **systems** that run every frame over
the entities that match their query, and in **observers** that react to events. Features are
grouped in **modules**, imported with `ecs::import<Module>()`.

Everything in the game follows this pattern, including the network and physics layers: there is
no hidden engine object with its own update loop.

## Server

`r-type_server` runs at 60 frames per second. Gameplay runs inside the physics fixed step, so the
simulation advances in identical 1/60 s ticks whatever the machine's speed.

| Module | Responsibility |
| --- | --- |
| `physics::World` (`libs/physics`) | Box2D bodies from `physics::Body`/`Box`/`Filter` components, the fixed step, contact events |
| `ServerMovement` | Creates a player per connection, turns `PlayerInput` into held commands that expire after 0.25 s, publishes `AuthoritativeMotion` |
| `ServerPlayers` | Free slot, spawn point, collider and health per player; the replicated match entity |
| `ServerCombat` | Fire rate, shots, hits, invulnerability, elimination, `Destroyed` events |
| `ServerWaves` | Seeded enemy waves while someone is alive, enemy fire at the nearest player |
| `ServerMatch` | Phases (waiting, playing, over), restart votes, departure notices |

Each fixed step runs three phases: **input** (commands, firing, waves, match rules), the Box2D
step, then **output** (positions read back, contacts turned into `Hit` events, despawns). Systems
declare what they read and write, and the SIECS scheduler orders them from those declarations.

The network transport has its own thread, which only moves UDP datagrams between the socket and
two lock-free queues. The game loop never waits for a client: it reads whatever arrived at the
start of a frame and queues what it sends at the end.

## Client

`r-type_client` runs at the screen's refresh rate (60 Hz by default).

| Module | Responsibility |
| --- | --- |
| `ray::Window`, `ray::Animations`, `ray::Audio`, `ray::Inputs` (`client/raylib`) | raylib window, sprites, cameras, animations, sounds and key bindings as ECS data |
| `gui::Gui` (`client/gui`) | raygui widgets as entities, laid out by systems |
| `scene` (`client/scene`) | A stack of scenes (menu, connecting, game, pause, options) that own their entities |
| `ClientMovement` | Local prediction of the controlled ship, interpolation of the others, trajectories of shots |
| `Visuals` | Sprites, callsigns, HUD lines, and the `MatchView` read by the UI |
| `Effects` | Hit flashes, explosions and synthesized sound effects |
| `Sessions` | Connection state, error messages, notices |
| `PreferencesModule` | Volume, accessibility options and key bindings, saved between runs |

### Hiding latency

- **Own ship:** the client runs the same movement rule as the server (`shared/movement.h`) on
  its own ship, so it moves the frame a key is pressed. When the server's position arrives, the
  client restarts from it and fades the difference out over 90 ms; a teleport (new `epoch`) snaps.
- **Other ships:** shown 100 ms in the past, interpolated between two server positions, so their
  motion is smooth despite packet jitter.
- **Shots and enemies:** they fly in straight lines, so the server sends their `Trajectory` once
  and the client computes their position on the same delayed timeline.
- **Explosions:** `Destroyed` carries the tick of the hit. The client keeps the body on its
  trajectory until the displayed time reaches that tick, then explodes it where it is drawn.

## Replication (`libs/net`)

A server entity is sent to clients when it has the `net::Replicated` tag. Its components
registered with `net::replicate<T>()` follow it: a spawn snapshot, then every `set()`. Reliable
components are resent until acknowledged; unreliable ones (like `AuthoritativeMotion`, sent every
tick) simply give way to the next value. Inputs (`net::input<T>()`) flow from the owning client
to the server, and events (`net::event<T>()`) are one-off messages in either direction. See the
[network protocol](protocol.md) for the bytes.

The schema in `shared/game.h` is registered identically on both sides; a client with a different
schema is refused at connection time.

## Match flow

```
 waiting --(a player joins)--> playing --(everyone eliminated)--> over
    ^                                                              |
    +----(last player leaves, arena cleared)    (everyone voted)---+--> playing (respawn)
```
