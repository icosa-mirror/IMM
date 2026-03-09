# IMM Viewer

A standalone Windows desktop player for `.imm` files.

## Contents

| File | Description |
|------|-------------|
| `appImmViewer_Release.exe` | The viewer executable |
| `settings.json` | Configuration file (edit this to change what file loads) |
| `viewer_settings.md` | Full reference for all settings.json fields |
| `*.dll` | Required runtime libraries — keep these alongside the exe |

## Quick start

1. Edit `settings.json` and set the path to your `.imm` file:
   ```json
   "File": {
     "Load": [ "C:/path/to/your/file.imm" ]
   }
   ```
2. Run `appImmViewer_Release.exe`.

## Loading a different settings file

You can pass an alternative settings file as a command line argument:

```
appImmViewer_Release.exe C:\path\to\my_settings.json
```

If no argument is given, the viewer looks for `settings.json` in the same directory as the exe.

## Desktop controls

### Camera movement

| Input | Action |
|-------|--------|
| `W` | Move forward |
| `S` | Move backward |
| `A` | Move left |
| `D` | Move right |
| `E` | Move up |
| `Q` | Move down |
| Hold `Shift` | Move 5× faster |
| Hold `Ctrl` | Move 5× slower |
| Left mouse drag | Look around |

### Playback

| Key | Action |
|-----|--------|
| `X` | Skip forward (next chapter) |
| `Z` | Skip back (previous chapter) |
| `C` | Restart from the beginning |
| `V` | Continue / replay |
| `P` | Pause / Resume |

### Spawn areas

An IMM file can contain named viewpoints called spawn areas. Use the shifted number keys to jump between them:

| Key | Action |
|-----|--------|
| `!` (Shift+1) | Jump to spawn area 0 |
| `@` (Shift+2) | Jump to spawn area 1 |
| `#` (Shift+3) | Jump to spawn area 2 |
| `$` (Shift+4) | Jump to spawn area 3 |
| … and so on up to `(` (Shift+9) for spawn area 8, `)` (Shift+0) for spawn area 9 |

## VR vs desktop

Two variants of this artifact are published, both containing the same executable. The only difference is the `EnableVR` value in `settings.json`:

- **ImmViewer-Windows** — `EnableVR` is `false`. Runs on any Windows PC.
- **ImmViewer-Windows-VR** — `EnableVR` is `true`. Requires an Oculus headset (Rift, Rift S, or Quest via Link/Air Link) with the Oculus PC app running.

You can switch between modes at any time by editing `settings.json`:
```json
"Rendering": {
  "EnableVR": true
}
```

## File paths in settings.json

Paths in `settings.json` are resolved relative to the exe's directory. For files outside that directory use an absolute path:

```json
"File": {
  "Load": [ "C:/Users/you/Documents/my_scene.imm" ]
}
```

## Troubleshooting

The viewer writes a log to `debug.txt` in the same directory as the exe. Check this file if the viewer fails to start or a file does not load — it contains renderer initialisation details, the settings file that was read, and any load errors.

## Full settings reference

See `viewer_settings.md` for documentation of every field in `settings.json`.
