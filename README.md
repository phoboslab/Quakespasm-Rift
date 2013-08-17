# Quake for Oculus Rift

[Download Here](https://github.com/phoboslab/Quakespasm-Rift/releases)

Based on [Quakespasm](http://quakespasm.sourceforge.net/). This enables support for the Oculus Rift for idsoftware's original Quake. You need the Oculus SDK and SDL to compile this.

More info: http://phoboslab.org/log/2013/07/quake-for-oculus-rift


Use ~ to access the console and type `vr_enabled 1` to activate the rift view. You may want to disable view bobbing with `cl_bob 0`.

## Additional cvars:
- `vr_supersample` – Supersampling. Default 2.
- `vr_prediction` – Prediction time in milliseconds. Default 40.
- `vr_driftcorrect` – Use magnetic drift correction. Default 1.
- `vr_crosshair` – 0: disabled, 1: point, 2: laser sight
- `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-5 pixels wide. Default 3.
- `vr_crosshair_depth` – Projection depth for the crosshair. Use `0` to automatically project on nearest wall/entity. Default 0.
- `vr_chromabr` – Use chromatic aberration compensation. Default 0.
- `vr_aimmode` – 1: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area. Default 1.
- `vr_deadzone` – Deadzone in degrees for `vr_aimmode 5`. Default 30.

