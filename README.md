# QUAKE for Oculus Rift

Enables support for the Oculus Rift for id Software's QUAKE. Based on [QuakeSpasm](http://quakespasm.sourceforge.net/).

More info: http://phoboslab.org/log/2013/07/quake-for-oculus-rift

Use ~ to access the console and type `vr_enabled 1` to activate the rift view. You may want to disable view bobbing with `cl_bob 0`. You can use `scr_showfps 1` to display the rendering frame rate.

## Primary VR cvars:
- `vr_enabled` – Enables or disables VR rendering and tracking input. Default 0.
- `vr_debug` – Determine what debug HMD to use when none are connected. 0: disables VR, 1: DK1, 2: DK2. Default 1.
- `vr_ipd` – Override the interpupillary distance in millimeters. Default 60.

## HMD cvars:
- `vr_multisample` – Multisample eye rendering by this multiplier. Try a value of 1.5 or 2. Default 1.
- `vr_lowpesistence` – Low persistence HMD mode. Default 0.
- `vr_dynamicprediction` – Dynamic prediction. Default 0.
- `vr_vsync` – Vertical sync control over the SDK distortion renderer. Default 0.

## Distortion cvars:
- `vr_chromatic` – Use chromatic aberration correction. Default 1.
- `vr_timewarp` – Time warping. Default 0.
- `vr_vignette` – Shader vignette. Currently appears busted in the SDK distortion renderer, tied to chromatic flagging. Default 1.

## Additional cvars:
- `vr_crosshair` – 0: disabled, 1: point, 2: laser sight
- `vr_crosshair_alpha` - Sets the alpha translucency of the crosshair dot/laser from 0 to 1. Default 0.25.
- `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-32 pixels wide. Default 3.
- `vr_crosshair_depth` – Projection depth for the crosshair in quake units. Use `0` to automatically project on nearest wall/entity. Default 0.
- `vr_aimmode` – 1: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area. 6: Identical to 5 with PITCH decoupled entirely. Default 1.
- `vr_deadzone` – Deadzone in degrees for `vr_aimmode 5/6`. Default 30. Setting to 180 will decouple the camera and aiming.

## Joystick cvars:
- `joy_sensitivity` – Sensitivity for the camera joystick axis. Default 32.
- `joy_filter` – Applies filtering to joystick axis, like mouse filtering. Default 1.
- `joy_deadzone` – Amount of deadzone for joystick axis. Default 0.125.
- `joy_function` – Easing functions for joystick axis. 0: linear, 1: sine, 2: quadratic, 3: cubic, 4: quartic, 5: quintic. Default 0.

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
