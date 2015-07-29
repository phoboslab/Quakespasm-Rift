# Quake for Oculus Rift

[Download Here](https://github.com/phoboslab/Quakespasm-Rift/releases)

Based on [Quakespasm](http://quakespasm.sourceforge.net/). This enables support for the Oculus Rift DK2 in Direct Mode for idsoftware's original Quake. You need the Oculus SDK 0.6 and SDL to compile this.

More info: http://phoboslab.org/log/2015/07/quake-for-oculus-dk2-direct-mode

The release version comes with a sensible `config.cfg` and `autoexec.cfg` file that enables VR Mode by default, disables view bobbing and sets the texture mode to NEAREST for the proper, pixelated oldschool vibe.

## Additional cvars:
- `vr_enabled` – 0: disabled, 1: enabled
- `vr_crosshair` – 0: disabled, 1: point, 2: laser sight
- `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-5 pixels wide. Default 3.
- `vr_crosshair_depth` – Projection depth for the crosshair. Use `0` to automatically project on nearest wall/entity. Default 0.
- `vr_aimmode` – 1: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area. Default 1.
- `vr_deadzone` – Deadzone in degrees for `vr_aimmode 5`. Default 30.

