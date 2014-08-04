# QUAKE for Oculus Rift

Enables support for the Oculus Rift for id Software's QUAKE. Based on [QuakeSpasm](http://quakespasm.sourceforge.net/).

More info: http://phoboslab.org/log/2013/07/quake-for-oculus-rift

Use ~ to access the console and type `vr_enabled 1` to activate the rift view. You may want to disable view bobbing with `cl_bob 0`.

## Additional cvars:
- `vr_ipd` – Override the interpupillary distance in millimeters. Default 60.
- `vr_multisample` – Multisample eye rendering by this multiplier. Default 1.
- `vr_lowpesistence` – Low persistence mode. Default 0.
- `vr_dynamicprediction` – Dynamic prediction. Default 0.
- `vr_vsync` – Vertical sync control of SDK distortion renderer. Default 0.
- `vr_chromatic` – Use chromatic aberration correction. Default 1.
- `vr_timewarp` – Time warping. Default 0.
- `vr_vignette` – Shader vignette. Currently appears busted in the SDK distortion renderer, tied to chromatic flagging. Default 1.
- `vr_crosshair` – 0: disabled, 1: point, 2: laser sight
- `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-5 pixels wide. Default 3.
- `vr_crosshair_depth` – Projection depth for the crosshair in units. Use `0` to automatically project on nearest wall/entity. Default 0.
- `vr_aimmode` – 1: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area. Default 1.
- `vr_deadzone` – Deadzone in degrees for `vr_aimmode 5`. Default 30. Setting to 180 will decouple the camera and aiming.

## Mac OS X

Includes an SDL.framework binary and Oculus SDK Xcode project for effortless building in Xcode.

## Linux and Windows

Building for Linux or Windows hasn't yet been updated to make use of the included Oculus SDK. They also require SDL.

LICENSE
=======

## QUAKE

Quake and QuakeSpasm are released under the [GNU GPL](http://www.gnu.org/copyleft/gpl.html).

## Oculus SDK

Includes the Oculus SDK (LibOVR) which is [under license from Oculus VR](http://developer.oculusvr.com/license).

## SDL

SDL is released under the [GNU LGPL](http://www.gnu.org/copyleft/lesser.html).
