# Accessibility

The subject asks what the game offers for each category of disability. This page lists what is
built in, how to use it, and what we know is still missing.

## Physical and motor disabilities

- **Remappable controls.** Every action (move up, down, back, forward, fire) can be bound to any
  key in **Options → Controls**, for example to play with one hand on the numeric keypad.
  Bindings are saved between runs.
- **Hold, don't tap.** Firing is automatic while the fire key is held; no action needs fast
  repeated presses or simultaneous keys.
- **Keyboard only.** Every screen works without a mouse: Enter joins and plays again, Escape
  pauses, goes back and cancels, M leaves the match. Mouse users can click every button instead.
- **Time to react.** After a hit, a ship cannot be hit again for one second.
- **No penalty for pausing.** The pause screen stops the ship from moving while the menu is open.

## Visual disabilities

- **Information never relies on colour alone.** Players are identified by number (P1–P4) under
  their ship and in the HUD as well as by ship colour. Health bars show the points left by
  their length as well as their colour, and an eliminated player is marked `ELIMINATED`.
- **Larger text.** **Options → Large text** raises the interface text size from 30 to 40 px.
- **High contrast.** The interface uses raygui's dark theme: light text on dark panels, over a
  black background with dim stars.
- **Resizable window.** The playfield scales to the window, so the game can fill a large screen.
- **Sound cues** accompany the main events (own shots, hits, explosions, game over) for players
  who see the screen poorly.

## Hearing disabilities

- **Every sound has a visual counterpart.** A hit makes the ship blink and lowers its health in
  the HUD; a destruction shows an explosion; game over shows a panel; departures show a written
  notice. Nothing in the game is conveyed by sound alone.
- **Volume** is adjustable from 0 (mute) to 100% in the options.

## Photosensitivity

- Hit flashes blink at most three times per second, the limit recommended by WCAG 2.3.1.
- **Options → Blink hit ships** turns blinking off entirely: hit ships
  stay steadily tinted red instead.

## Mental and cognitive disabilities

- **One goal, few rules.** One weapon, one enemy type, three health points; the [rules](playing.md)
  fit in a few lines.
- **Controls one menu away.** **Options** lists the current keys, updated when they are
  remapped. Each menu shows its keyboard shortcuts.
- **Plain messages.** Connection problems are explained in words (*the match is full*, *nothing
  answered at this address*), never as error codes.
- **Cooperative, no punishment for losing.** Players cannot hurt each other; an eliminated player
  keeps watching the team, and everyone restarts together.

## Known gaps

- The interface text is in English only.
- There is no gamepad support yet, and no difficulty setting.
- The interface is not exposed to screen readers.

## Documentation

These pages are plain Markdown with headings, tables and text alternatives (no information is
given only by an image), readable by screen readers on GitHub or once built with MkDocs.
