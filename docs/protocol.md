# Morgane network protocol (version 1)

**Status:** describes the protocol implemented by `libs/net` (`sinet`) and the game schema in
`shared/game.h`. The key words MUST, MUST NOT, SHOULD and MAY are used as in RFC 2119.

## 1. Overview

Clients and the server exchange binary datagrams over **UDP**. The server listens on one port
(4242 by default); a client sends from any port. Each datagram is one **packet**: a fixed 38-byte
header followed by **frames**. Packets are at most **1200 bytes**, header included, so they are
never fragmented by IP on common networks.

A session goes through these steps:

1. The client sends `hello` with its schema until the server answers.
2. The server answers `welcome` (accepted) or `reject` (refused, with a reason).
3. The server sends `spawn`, `component`, `relations`, `despawn` and `event` frames describing
   the game; the client sends `component` frames for its inputs and `event` frames.
4. Either side sends `disconnect`, or stops hearing from the other for 5 seconds.

## 2. Conventions

- All integers are **little-endian** and unsigned unless stated otherwise.
- `f32` is an IEEE 754 binary32 float, little-endian. `bool` is one byte, 0 or 1; any other value
  makes the value invalid.
- Sequence numbers are `u32` and wrap around: `a` is newer than `b` when the signed 32-bit
  difference `a - b` is positive.
- An **entity id** is a `u32`: `(generation << 16) | index`, with `index` in `1..max_entities`.
  Id 0 means "no entity". A reused index gets a new generation.

## 3. Packet header

| Offset | Size | Field | Description |
| --- | --- | --- | --- |
| 0 | 4 | `magic` | `0x53494E45` (bytes `45 4E 49 53`) |
| 4 | 2 | `version` | `1` |
| 6 | 2 | `flags` | bit 0: packet is reliable. Other bits MUST be 0 |
| 8 | 8 | `session` | Random non-zero token chosen by the client for this connection |
| 16 | 4 | `sequence` | Sender's packet number, starting at 1, +1 per packet sent |
| 20 | 4 | `ack` | Newest `sequence` received from the other side (0: none yet) |
| 24 | 8 | `ack_bits` | Bit `i` set: packet `ack - (i + 1)` was also received |
| 32 | 4 | `reliable_sequence` | Reliable packets: 1, 2, 3… per direction. Unreliable: 0 |
| 36 | 2 | `payload_size` | Bytes after the header; MUST equal the datagram size minus 38 |

A receiver MUST drop the packet when the magic or version differ, `flags` has unknown bits,
`sequence` is 0, the reliable bit and `reliable_sequence` disagree, `payload_size` is wrong, or
any frame is malformed (section 4). Packets whose `session` does not match the connection's are
ignored, except `hello` (section 6).

## 4. Frames

The payload is a sequence of frames:

| Size | Field |
| --- | --- |
| 1 | `op` |
| 2 | `size`: length of `body` |
| `size` | `body` |

| `op` | Name | Direction | Body |
| --- | --- | --- | --- |
| 0 | `hello` | C → S | Schema (section 6) |
| 1 | `welcome` | S → C | `u32 peer_id`, `u16 max_entities`, schema |
| 2 | `reject` | S → C | `u8 reason` |
| 3 | `heartbeat` | both | empty |
| 4 | `disconnect` | both | empty |
| 5 | `spawn` | S → C | Entity record (section 7.1) |
| 6 | `despawn` | S → C | `u32 entity` |
| 7 | `relations` | S → C | Entity record (section 7.1) |
| 8 | `component` | both | `u32 entity`, `u16 slot`, `u32 revision`, value |
| 9 | `remove` | both | `u32 entity`, `u16 slot`, `u32 revision` |
| 10 | `event` | both | `u32 entity`, `u16 slot`, `u32 revision`, value |

An `op` above 10, or a `size` running past the payload, makes the whole packet invalid. A frame
whose body does not have the expected length for its type is ignored. Frames are applied in
order; frames of one packet never span two packets.

**Reject and disconnect reasons** (`u8`): 0 remote (the other side closed), 1 timeout,
2 schema mismatch, 3 capacity (the server is full), 4 congestion (the receiver could not keep up).

## 5. Reliability

- Every packet carries acknowledgements (`ack`, `ack_bits`) for the last 65 packets received.
- A packet with the reliable flag MUST be resent, with a new `sequence` but the same
  `reliable_sequence`, until one of its sequences is acknowledged. The implementation resends
  after `max(50 ms, 1.5 × round-trip time)` and keeps at most 64 reliable packets in flight.
- A receiver delivers reliable packets in `reliable_sequence` order, buffering at most 64 ahead
  of the next expected one and dropping packets further ahead. Duplicates are dropped.
- Unreliable packets are applied when they arrive, unless their `sequence` was already received.
- A side with nothing to send SHOULD send a `heartbeat` every 500 ms, and promptly when it owes
  acknowledgements. A side that receives nothing for **5 seconds** considers the connection lost.

## 6. Connection

The **schema** describes the network types both sides use, in slot order:

| Size | Field |
| --- | --- |
| 2 | `count` |
| 16 × `count` | `u64 type_id`, `u64 schema_hash` for each slot 0…count-1 |

1. The client picks a random non-zero `session` and sends an **unreliable** packet holding one
   `hello` frame with its schema, every 250 ms until it gets an answer.
2. If the schema differs from the server's, the server answers `reject` with reason 2. If four
   players are connected, it answers `reject` with reason 3. A `reject` packet uses the client's
   `session` and `sequence` 1.
3. Otherwise the server answers a **reliable** packet holding `welcome`: the client's `peer_id`
   (non-zero), the server's `max_entities` (4096) and the server's schema. The client MUST check
   the schema again and that `max_entities` fits its own tables.
4. A `hello` with a new `session` from an address and port that already has a session is ignored
   while that session is live. Once the old session has been silent for 1 second (a connected
   peer sends at least a heartbeat every 500 ms), the next `hello` closes it (as if it had sent
   `disconnect`) and opens the new one. A delayed `hello` from an older session, or a forged one,
   therefore cannot end a live session. The client keeps retrying its `hello` until then.

Leaving: send an unreliable packet with a `disconnect` frame, also while the handshake is still
running, since the server may already have accepted the `hello`. The server then announces the
departure to the other players (`PlayerLeft`, section 8).

## 7. Replication (server → client)

### 7.1 Entities

`spawn` and `relations` carry an **entity record** (24 bytes):

| Size | Field |
| --- | --- |
| 4 | `entity`: entity id |
| 4 | `revision`: increases with every change of this record |
| 4 | `owner`: `peer_id` of the controlling client, 0 if none |
| 4 | `lobby`, `game`, `scope`: grouping ids, 0 in this game |

`spawn` creates the entity on the client (replacing any older entity with the same index);
`relations` updates its record when `revision` is newer than the last one applied. The entity
whose `owner` equals the client's `peer_id` is the client's own ship.

`despawn` deletes the entity. Right after `spawn`, the server sends a `component` frame for each
type the entity has.

### 7.2 Components

`component` sets the value of type `slot` on `entity`; `remove` deletes it. A client MUST ignore
a frame whose `revision` is not newer than the last one it applied for that entity and slot,
whose entity is unknown, or whose slot is not a replicated type.

### 7.3 Inputs and events

- A client sends its inputs as `component` frames on the entity it owns. The server ignores
  inputs for entities the client does not own.
- `event` frames carry a one-off message of type `slot`. On entity 0, a client event targets the
  sender's own connection; a server event targets the receiving client. `revision` is not used
  by events and SHOULD be 0.

## 8. Game schema

Slots are the types sorted by `type_id`. Values are the fields below, packed in this order with
no padding. `r-type_bot --schema` prints this table from the code.

| Slot | Type | `type_id` | `schema_hash` | Kind | Size | Fields |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | `game::Destroyed` | `0x185273895c9a2366` | `0xac932336c39c1bf1` | server event, reliable | 8 | `u64 tick` |
| 1 | `game::Health` | `0x4865173c81f94251` | `0x881880e10b8e0d84` | component, reliable | 2 | `u8 current`, `u8 max` |
| 2 | `game::PlayerInput` | `0x6bfb179a2a87cb7a` | `0x87d2810a473f52d5` | input, unreliable | 9 | `f32 move_x`, `f32 move_y`, `bool fire` |
| 3 | `game::Match` | `0x741ab879a1e07000` | `0xc2ef584e490f8176` | component, reliable | 7 | `u8 phase`, `u8 players`, `u8 ready`, `u32 wave` |
| 4 | `game::PlayerLeft` | `0x7c9268b5d85e21a3` | `0xdb0596a28b56c044` | server event, reliable | 2 | `u8 slot`, `u8 reason` |
| 5 | `game::EntityKind` | `0x8dca40a171e0aa42` | `0x3f112430326df30a` | component, reliable | 1 | `u8 value` |
| 6 | `game::RestartVote` | `0xa4d713e6e2ecedb2` | `0x31e68b3a1af44705` | client event, reliable | 1 | `bool ready` |
| 7 | `game::PlayerSlot` | `0xba602c0eed253a82` | `0x7171c1e73ebbd7aa` | component, reliable | 1 | `u8 index` |
| 8 | `game::Trajectory` | `0xcc1648d01eac5576` | `0x0deb3cd84da18a99` | component, reliable | 24 | `u64 tick`, `f32 x`, `f32 y`, `f32 vx`, `f32 vy` |
| 9 | `game::AuthoritativeMotion` | `0xf4c2cbc1b6d7496c` | `0x7dcff2f10f7fabc1` | component, unreliable | 28 | `u64 tick`, `f32 x`, `f32 y`, `f32 vx`, `f32 vy`, `u32 epoch` |

`type_id` is the 64-bit FNV-1a hash of the type name. `schema_hash` also covers the field names,
types and sizes, the direction, the reliability and the size; treat it as an opaque constant.

### 8.1 Meaning

- **World.** The playfield is 640 × 360 units centred on (0, 0); x grows to the right, y grows
  downwards. Speeds are in units per second. The simulation advances in ticks of 1/60 s.
- **Match** is on one entity. `phase`: 0 waiting for players, 1 playing, 2 over. `players` is the
  number of players, `ready` how many voted to play again, `wave` the current wave.
- **Players** are the entities with `PlayerSlot` (`index` 0–3, shown as P1–P4). They also have
  `Health` and `AuthoritativeMotion`: the position (`x`, `y`) and velocity at the end of `tick`.
  `epoch` changes when the position jumps (respawn), so clients should snap instead of smoothing.
  A player with `Health.current` 0 is eliminated.
- **Shots and enemies** have `EntityKind` (0 player shot, 1 enemy, 2 enemy shot) and a
  `Trajectory`: at tick `t ≥ tick`, the position is `(x + vx·(t − tick)/60, y + vy·(t − tick)/60)`.
- **PlayerInput** is sent by a client on its own player, at least every 0.25 s while a key is
  held: `move_x`, `move_y` in [-1, 1] (left/up negative), `fire` while the fire key is held.
  Values outside that range are treated as no movement.
- **RestartVote** is sent on entity 0 during phase 2; `ready` true votes to play again, false
  withdraws the vote. When every player has voted, the server respawns everyone.
- **Destroyed** is sent on a shot or enemy destroyed by a hit at `tick`, just before its
  `despawn`. **PlayerLeft** is sent on the match entity when a player's connection ends, with the
  disconnect `reason`.

## 9. Example

A client joining an empty server, abridged:

```
C → S  [hdr session=S seq=1 unreliable] hello{count=10, (type_id, schema_hash) × 10}
S → C  [hdr session=S seq=1 rel=1]      welcome{peer_id=1, max_entities=4096, schema}
S → C  [hdr session=S seq=2 rel=2]      spawn{entity=0x00010001, owner=0}
                                        component{entity=0x00010001, slot=3 Match, {1,1,0,0}}
                                        spawn{entity=0x00010002, owner=1}         ← your ship
                                        component{entity=0x00010002, slot=7 PlayerSlot, {0}}
                                        component{entity=0x00010002, slot=1 Health, {3,3}}
C → S  [hdr session=S seq=2 ack=2]      component{entity=0x00010002, slot=2 PlayerInput,
                                                  {0.0, -1.0, true}}
S → C  [hdr session=S seq=3 unreliable] component{entity=0x00010002, slot=9 AuthoritativeMotion, …}
```

## 10. Security considerations

The server trusts nothing from clients: it validates every frame's length and slot, accepts
inputs only on the sender's own entity and only for input types, clamps movement, and decides
all hits, deaths and spawns itself. The session token protects against stray or stale packets,
not against an attacker who can read the traffic; the protocol has no encryption or
authentication.
