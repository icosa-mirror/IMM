# Viewer settings.json

This document describes the `settings.json` format used by the `appImmViewer` executable.

Location
- Default file lives at `code/appImmViewer/exe/settings.json`.
- The viewer reads `settings.json` from its working directory at startup.

Top-level structure
```
{
  "Rendering": { ... },
  "File": { ... },
  "Window": { ... },
  "Playback": { ... },
  "Sound": { ... },
  "UI": { ... }
}
```

Transform object
A transform is a JSON object with:
```
{
  "Rotation": [x, y, z, w],
  "Scale": number,
  "Flip": "N" | "X" | "Y" | "Z",
  "Translation": [x, y, z]
}
```

Rendering
- `EnableVR` (bool): enable VR rendering. Default: `true`.
- `RenderingAPI` (string): `"OpenGL"` or `"DirectX"`. Default: `"OpenGL"`.
- `RenderingTechnique` (string): `"Static"` or `"Pretessellated"`. Default: `"Static"`.
- `PixelDensity` (number): default `1.0`.
- `Supersampling` (number, int): default `1`.

File
- `Load` (array of strings): list of `.imm` files to load. In packaged viewer artifacts, `sample1.imm` is staged beside the executable and settings files use `"sample1.imm"`.

Window
- `FullScreen` (bool): default `false`.
- `PositionX` (number, int): default `0`.
- `PositionY` (number, int): default `0`.
- `Width` (number, int): default `1920`.
- `Height` (number, int): default `1080`.

Playback
- `Location` (transform): world-space location for playback.
- `PlayerSpawn.Location` (string): spawn area name. Default: `"Spaces_Player1"`.
- `PlayerSpawn.Custom` (transform): optional; defaults to identity if missing.

Sound
- `Device` (string): audio device name. Optional.

UI
- `EnableHaptics` (bool): default `true`.
- `LeftHanded` (bool): default `false`.
- `UISoundVolume` (number): default `0.5`.

Example
```
{
  "Rendering": {
    "EnableVR": true,
    "RenderingAPI": "OpenGL",
    "RenderingTechnique": "Static",
    "PixelDensity": 1.0,
    "Supersampling": 1
  },
  "File": {
    "Load": [ "sample1.imm" ]
  },
  "Window": {
    "FullScreen": false,
    "PositionX": 0,
    "PositionY": 0,
    "Width": 1920,
    "Height": 1080
  },
  "Playback": {
    "Location": {
      "Rotation": [ 0, 0, 0, 1 ],
      "Scale": 1,
      "Flip": "N",
      "Translation": [ 0, 0, 0 ]
    },
    "PlayerSpawn": {
      "Custom": {
        "Rotation": [ 0, 0, 0, 1 ],
        "Scale": 1,
        "Flip": "N",
        "Translation": [ 0, 0, 0 ]
      },
      "Location": "Default"
    }
  },
  "Sound": {
    "Device": "Default"
  },
  "UI": {
    "EnableHaptics": true,
    "LeftHanded": false,
    "UISoundVolume": 0.5
  }
}
```
