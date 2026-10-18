# Playing

The in-game text is in French; this page gives each label with its translation.

## Joining

1. Start the client. The menu (**MORGANE**) shows the server address, as `address:port`.
2. Type the address if needed, then press **Enter** or click **Rejoindre** (join).
3. **Connexion…** (connecting) shows while the server answers. **Annuler** or Escape cancels.

The match starts as soon as the first player arrives; others can join any of the four places
while it runs. If the connection fails, the menu explains why:

| Message | Meaning |
| --- | --- |
| *Partie pleine : 4 joueurs maximum.* | The match already has four players. |
| *Aucun serveur ne repond a …* | Nothing answered at that address within five seconds. |
| *Connexion perdue : le serveur ne repond plus.* | The server stopped answering during the match. |
| *Le serveur utilise une autre version du jeu.* | Client and server were built from different network schemas. |
| *Adresse inconnue* / *Port invalide* | The address could not be resolved or the port is not a number. |

## Controls

| Action | Keys (default) |
| --- | --- |
| Move | Arrows or W A S D |
| Fire (hold) | Space |
| Pause menu | Escape |
| Play again, after a game over | Enter |
| Leave to the menu, from pause or game over | M |

The bottom-left corner of the game always shows the current keys. Every key can be changed in
**Options** (see below). Every menu can be used with the keyboard alone.

## Rules

- Each player has three health points (**PV**). An enemy shot or touching an enemy costs one; a
  ship blinks and cannot be hit again for one second after a hit.
- One shot destroys an enemy. Shots never hurt other players.
- Enemies come in waves of five from the right, at random heights, and fire at the nearest ship.
- At zero health a ship explodes and its player watches the others (*Vous etes elimine*).
- When every ship is destroyed, **PARTIE TERMINEE** (game over) appears. Each player presses
  **Rejouer** (play again) or Enter; once everyone has, all ships respawn and the waves start
  again. A player who joins during a game over waits for the next match.
- Leaving or losing the connection frees the place. The others see *P2 a quitte la partie*
  (left) or *P2 a perdu la connexion* (connection lost) and keep playing.

## Screen

- Top left: one line per player, `P1  PV 3/3`, `ELIMINE` when out, `(vous)` for you.
- Top right: the wave number and the number of players.
- Under each ship: its player number. Ships also differ by colour.
- Top centre: short notices, such as departures.

## Options

Open **Options** from the menu or the pause screen. Changes apply at once and are saved in
`~/.config/morgane/preferences.txt`.

| Option | Effect |
| --- | --- |
| Volume | Master volume, 0 mutes the game |
| *Faire clignoter les vaisseaux touches* | Hit ships blink (at most three times per second). Off: they stay tinted red instead |
| *Texte agrandi* | Larger interface text |
| Commandes | Click an action, then press its new key. Escape cancels. *Touches par defaut* restores the defaults |
