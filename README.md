# Quake for Oculus Rift

[Download Here](https://github.com/phoboslab/Quakespasm-Rift/releases)

Based on [Quakespasm](http://quakespasm.sourceforge.net/). This enables support for the Oculus Rift CV1 in Direct Mode for idsoftware's original Quake. You need the Oculus SDK 1.9.0 and SDL to compile this.

More info: http://phoboslab.org/log/2016/05/quake-for-oculus-rift

The release version comes with a sensible `config.cfg` and `autoexec.cfg` file that enables VR Mode by default, disables view bobbing and sets the texture mode to NEAREST for the proper, pixelated oldschool vibe.

## Building:

1. Install the free Visual Studio 2015 Community Edition, if you don't already have it.

2. Go to https://developer.oculus.com/downloads/pc/1.9.0/Oculus_SDK_for_Windows/ then download and extract it NEXT TO (not in) your Quakespasm-Rift folder. So there is an "OculusSDK" folder and a "QuakeSpasm-Rift" folder side-by-side.

3. Use the VC13 solution to compile what you need:
	Quakespasm-Rift\Windows\VisualStudio\quakespasm.sln
	
4. In Visual Studio, right click project quakespasm-sdl2, click Properties. Set Configuration to All Configurations. Choose Debugging, set Working Directory to your Quake folder.

5. Run the quakespasm-sdl2 project. You need to enable VR in the game options and start a new game before it will be in VR.

## Additional cvars:
- `vr_enabled` – 0: disabled, 1: enabled
- `vr_crosshair` – 0: disabled, 1: point, 2: laser sight
- `vr_crosshair_size` - Sets the diameter of the crosshair dot/laser from 1-32 pixels wide. Default 3.
- `vr_crosshair_depth` – Projection depth for the crosshair. Use `0` to automatically project on nearest wall/entity. Default 0.
- `vr_crosshair_alpha` – Sets the opacity for the crosshair dot/laser. Default 0.25.
- `vr_aimmode` – 1: Head Aiming, 2: Head Aiming + mouse pitch, 3: Mouse aiming, 4: Mouse aiming + mouse pitch, 5: Mouse aims, with YAW decoupled for limited area, 6: Mouse aims, with YAW decoupled for limited area and pitch decoupled completely. Default 1.
- `vr_deadzone` – Deadzone in degrees for `vr_aimmode 5`. Default 30.
- `vr_perfhud- – Show the Oculus Performance Hud (1-5). Default 0.
- `snd_device` – Search string for the audio output device to use. Default: "default". This will get set automatically to the Oculus "Rift Audio" when `vr_enabled` is 1
