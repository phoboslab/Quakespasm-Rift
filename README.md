# Quake for Oculus Rift

[Download Here](https://github.com/phoboslab/Quakespasm-Rift/releases)

Based on [Quakespasm](http://quakespasm.sourceforge.net/). This enables support for the Oculus Rift for idsoftware's original Quake. You need the Oculus SDK and SDL to compile this.

More info: http://phoboslab.org/log/2013/07/quake-for-oculus-rift


Use ~ to access the console and type `r_oculusrift 1` to activate the rift view. You may want to disable view bobbing with `cl_bob 0`.

## Additional cvars:
- `r_oculusrift_supersample` – Supersampling. Default 2.
- `r_oculusrift_prediction` – Prediction time in milliseconds. Default 40.
- `r_oculusrift_driftcorrect` – Use magnetic drift correction. Default 1.
- `r_oculusrift_crosshair` – 0: disabled, 1: point, 2: laser sight
- `r_oculusrift_chromabr` – Use chromatic aberration compensation. Default 0.
- `r_oculusrift_aimmode` – 1: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch. Default 1.

