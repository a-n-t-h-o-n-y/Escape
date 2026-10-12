# Demos

Build in Release mode, then run these from the project directory.

## Lumen

```sh
./build/examples/escape.examples.lumen
```

Four full-window, true-color scenes. Use Tab/Shift+Tab, Left/Right, `1`–`4`,
or click a tab. Space pauses; `q` quits.

| Scene | Controls |
| --- | --- |
| Liquid | Move to steer the comet; click for ripples; wheel changes color |
| Tide | Move to disturb the water; click to splash; wheel changes strength |
| Warp | Move to steer; click to boost; wheel changes speed |
| Julia | Left-drag to morph; wheel up zooms in at the pointer, down zooms out |

Scenes keep their state between visits. Julia uses more iterations and extended
precision for deep views; floating-point precision still limits visible detail.

Lumen explicitly requests true color and mouse motion, even with `NO_COLOR` set.
Large frames render rows in parallel and repaint changed cells. There is no
window-size cap; frame rate still depends on the terminal and the machine.
Synchronized output is used when the terminal confirms support.

## Pong

```sh
./build/examples/escape.examples.pong
```

A terminal Pong game with keyboard and AI players. Use the on-screen menu to
choose a game. The terminal must be at least 83 columns by 29 rows.

## Capability tester

```sh
./build/tools/termcaps
```

Try colors, text, input, cursor controls, links, sounds, and mode queries in a real
terminal. See the [termcaps controls](termcaps.md). Queries record replies;
visual samples are for you to judge.

## Host event loop

[examples/event-loop.cpp](../examples/event-loop.cpp) shows how to combine
Escape's input source and deadline with a host event loop. The example handles
both POSIX descriptors and Windows handles.
