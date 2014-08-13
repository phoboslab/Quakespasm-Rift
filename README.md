# QUAKE for Oculus Rift

Enables support for the Oculus Rift for id Software's QUAKE. Based on [QuakeSpasm-Rift](https://github.com/phoboslab/Quakespasm-Rift) by [phoboslab](http://phoboslab.org/log/2013/07/quake-for-oculus-rift) and [QuakeSpasm](http://quakespasm.sourceforge.net/). Special thanks to [dghost](https://github.com/dghost) for [Quake II VR](https://github.com/q2vr/quake2vr) and its code, which sped up development.

This fork is focusing on extending features related to Oculus Rift DK2 support (while maintaining Rift DK1 support).

You can attempt to start up the game directly in VR mode with the following command line:

- `quakespasm -vr`

To dismiss the `HEALTH & SAFETY` warning, simply tap on your HMD, or wait a few seconds.

Supports 2D menus and includes an in-game `VR/HMD Options` menu to toggle most VR settings.

Toggle the console by using the `~` key. The console allows you to enter commands and change console variables (cvars).

## Suggested cvars:
- `cl_bob 0` – Disable the original in-game view bobbing.
- `r_texturemode GL_NEAREST` – Favors pixellated vs. "blurry" textures. Harkens back to the software rendering roots and makes the low-resolution textures look sharper to my eyes.
- `r_particles 2` – Renders particles as squares instead of circles. Also harkens back to the software rendering roots.
- `host_maxfps 240` – Allows the game to run at up to 240 frames per second, given your computer has the capabilities and that `vid_vsync` or `vr_vsync` are disabled. Enabling `vsync` will cause the game to run at the monitor refresh rate (traditionally 60Hz, or 75Hz with the Oculus Rift DK2).
- `scr_showfps 1` – Display the rendering frame rate. You can use this to verify that an Oculus Rift DK2 is outputting 75 Hz with `vr_vsync` enabled. Can also help you tune `vr_multisample` (explained below) if your computer isn't capable of multisampling 2x. Simply set `scr_showfps 0` to disable.
- `scr_menuscale 2` – Scales the menu up 2x. You can also adjust this via the `Options` menu via the `Scale` slider.

## Primary VR cvars:
- `vr_enabled` – Enables or disables VR rendering and tracking input. Default 0.
- `vr_debug` – Determine what debug HMD to use when none are connected. 0: disables VR, 1: DK1, 2: DK2. Default 1.
- `vr_ipd` – Override the interpupillary distance in millimeters. Default 60.
- `vr_position` – Configure position tracking. 0: off, 1: default (camera-only, detached view model), 2: view entity. Default 1.
- `vr_reset` – This is a command that takes no arguments. It resets the player to a 'home' position according to your offset to the camera. You can bind this to a key or joystick button with `bind xxx vr_reset` (where xxx is a key or JOYx/AUXx button).

## HMD cvars:
- `vr_multisample` – Multisample eye rendering by this multiplier. Try a value of 1.5 or 2. Default 1.
- `vr_lowpersistence` – Low persistence HMD mode. Default 0.
- `vr_dynamicprediction` – Dynamic prediction. Default 0.
- `vr_vsync` – Vertical sync control over the SDK distortion renderer. Default 0.

## Distortion cvars:
- `vr_chromatic` – Use chromatic aberration correction. Default 1.
- `vr_timewarp` – Time warping. Default 0.
- `vr_vignette` – Shader vignette. Default 1.

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
- `joy_axis_debug` – Print the axis index number and values, with respect to the deadzone. Look at the values for a clue as to which axis is dominant in order to identify the axis "index number". Default 0.
- `joy_axismove_x` – Set the axis index for strafing/side-stepping left & right. Default 0.
- `joy_axismove_y` – Set the axis index for moving forwards & backwards. Default 1.
- `joy_axislook_x` – Set the axis index for turning/looking left & right. Default 2.
- `joy_axislook_y` – Set the axis index for looking up & down. You must also turn `Mouse Look` ON in the settings menu! Default 3.

## Mac OS X

Includes an SDL.framework binary and Oculus SDK Xcode project for effortless building in Xcode.

## Linux and Windows

The Windows Visual Studio solution integrates, builds and links against the provided Oculus SDK.

Building for Linux or Windows will require you to provide the SDL 1.2.15 headers + libaries and configure path(s) in the respective development/build environments.

LICENSE
=======

## QUAKE

Quake and QuakeSpasm are released under the [GNU GPL](http://www.gnu.org/copyleft/gpl.html).

## Oculus SDK

Includes the Oculus SDK (LibOVR) which is [under license from Oculus VR](http://developer.oculusvr.com/license).

## SDL

SDL is released under the [GNU LGPL](http://www.gnu.org/copyleft/lesser.html).
