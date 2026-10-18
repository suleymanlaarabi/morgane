# Playing

This page describes the in-game screens and labels.

## Joining

1. Start the client. The menu (**MORGANE**) shows the server address, as `address:port`.
2. Type the address if needed, then press **Enter** or click **Join**.
3. **CONNECTING** shows while the server answers. **Cancel** or Escape cancels.

The match starts as soon as the first player arrives; others can join any of the four places
while it runs. If the connection fails, the menu explains why:

| Message | Meaning |
| --- | --- |
| *Match full: 4 players maximum.* | The match already has four players. |
| *No server answers at …* | Nothing answered at that address within five seconds. |
| *Connection lost: the server stopped answering.* | The server stopped answering during the match. |
| *The server runs a different game version.* | Client and server were built from different network schemas. |
| *Unknown address* / *Invalid port* | The address could not be resolved or the port is not a number. |

## Controls

| Action | Keys (default) |
| --- | --- |
| Move | Arrows or W A S D |
| Fire (hold) | Space |
| Pause menu | Escape |
| Play again, after a game over | Enter |
| Leave to the menu, from pause or game over | M |

Every key can be changed in **Options** (see below), which also lists the current keys. Every
menu can be used with the keyboard alone.

## Rules

- Each player has three health points (**HP**). An enemy shot or touching an enemy costs one; a
  ship blinks and cannot be hit again for one second after a hit.
- One shot destroys an enemy. Shots never hurt other players.
- Enemies come in waves of five from the right, at random heights, and fire at the nearest ship.
- At zero health a ship explodes and its player watches the others (*You are eliminated*).
- When every ship is destroyed, **GAME OVER** appears. Each player presses
  **Play again** or Enter; once everyone has, all ships respawn and the waves start
  again. A player who joins during a game over waits for the next match.
- Leaving or losing the connection frees the place. The others see *P2 left the match*
  or *P2 lost connection* and keep playing.

## Screen

- Top left: one line per player, a health bar (green, yellow, then red on the last point) followed
  by `P1`, `(you)` for you and `ELIMINATED` when out.
- Top right: the wave number and the number of players.
- Under each ship: its player number. Ships also differ by colour.
- Top centre: short notices, such as departures.

## Options

Open **Options** from the menu or the pause screen. Changes apply at once and are saved in
`~/.config/morgane/preferences.txt`.

| Option | Effect |
| --- | --- |
| Volume | Master volume, 0 mutes the game |
| *Blink hit ships* | Hit ships blink (at most three times per second). Off: they stay tinted red instead |
| *Large text* | Larger interface text |
| Controls | Click an action, then press its new key. Escape cancels. *Reset to defaults* restores the defaults |
