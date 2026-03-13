Simple Camera for Advanced Filming & Photography

Supports GTA 5 Legacy & FiveM and GTA 5 Enhanced.

This mod provides a comprehensive cinematic toolkit for advanced camera control
and world manipulation. It features a seamless 6-axis free camera with both
Euler and Quaternion rotation modes, alongside a physics based drone mode that
simulates FPV flight with configurable acceleration, drag, and gravity. Users
can benefit from intelligent camera collisions, dynamic entity tracking with
visual markers, and a rigid mounting system that inherits the movement of peds
or vehicles.

The suite includes various lens settings, such as dynamic FOV adjustment and
manual ingame depth of field controls with autofocus. To ensure a clean
workspace, the interface automatically toggles HUD and player visibility. 
Beyond the camera itself, the mod offers total control over the environment
through time freezing, weather blending, and timelapse modes. It is also fully
integrated with IGCS tools, allowing for advanced depth of field rendering for
high-end virtual photography.

IGCS Connector & Depth Of Field Guide (Singleplayer)
----------------------------------------------------
1. Verify you have installed ReShade with full Add-on support.
2. Place `IgcsConnector.addon64` into your game directory alongside GTA5.exe.
3. Install the `IgcsDof.fx` shader into your reshade-shaders/shaders folder. 
4. Launch the game, open the Free Camera menu [F5], and activate free camera mode. World freeze is automatically enabled when starting a DoF session.
5. Open your ReShade overlay and activate the `IgcsDof` shader.
6. Head to the ReShade "Addons" tab and click "Start depth-of-field session". Simple Camera controls will be frozen and IGCS will take full control.
7. Set your max bokeh size first. The image will split into two (this is normal).
8. Enable "Show magnifier" and point it to the subject in the preview image.
9. Tweak the "Focus delta X" slider until the image in the magnifier is sharp and unified.
10. Adjust bokeh quality settings and click "Start render" when ready.

IGCS Connector & Depth Of Field Guide (FiveM)
---------------------------------------------
1. Verify you have installed ReShade for FiveM with full Add-on support.
2. Place `IgcsConnector.addon64` into your `FiveM\FiveM.app\plugins` folder.
3. Install the `IgcsDof.fx` shader into your FiveM reshade-shaders/shaders folder.
4. Follow steps 4-10 from the Singleplayer guide above to operate the camera and render your photo.

Installation (Singleplayer)
---------------------------
1. Install Alexander Blade's ScriptHookV & ASI Loader.
2. Place `SimpleCamera.asi` into your main Grand Theft Auto V directory (alongside `GTA5.exe`).
3. (Optional) For IGCS support, install ReShade with full Add-on support, then place `IgcsConnector.addon64` into the directory, also install `IgcsDof.fx` shader into your reshade-shaders/shaders folder..
4. (Optional) Edit `SimpleCamera.ini` to change your menu toggle hotkey (Default is F5).

Installation (FiveM)
--------------------
1. Copy `SimpleCamera.asi` into your `FiveM\FiveM.app\plugins` folder.
2. (Optional) Copy `SimpleCamera.ini` into the same folder to change your menu toggle hotkey.
3. (Optional) For IGCS support, verify ReShade is installed for FiveM with Add-on support. Place `IgcsConnector.addon64` into your `plugins` directory, and install the `IgcsDof.fx` shader into your ReShade shaders folder.

Controls Reference
------------------
Activation
  [F5]              - Open/Close the Simple Camera Menu

Keyboard Controls
  W / A / S / D     - Move Camera Forward / Left / Right / Backward
  Space / Ctrl      - Move Camera Up / Down
  Q / E             - Roll Camera Left / Right
  Mouse Movement    - Look Around (Pitch and Yaw)
  + / -             - Zoom In / Out (Field of View)
  Mouse Scroll      - Dynamically Adjust Movement Speed
  Esc               - Pause Game

Controller Controls
  Left Stick        - Move Camera Forward / Left / Right / Backward
  Right Stick       - Look Around (Pitch and Yaw)
  LT / RT (Triggers)- Move Camera Down / Up
  LB / RB (Bumpers) - Roll Camera Left / Right
  D-Pad Up / Down   - Zoom In / Out (Field of View)

Menu Navigation
  Numpad 8 / 2      - Move Cursor Up / Down
  Numpad 4 / 6      - Change Setting Values
  Numpad 5 / Enter  - Select Option / Toggle Feature / Type Exact Decimal Value
  Numpad 0 / Back   - Return / Close Menu
  
  Controller D-Pad  - Move Cursor
  Controller A      - Select / Toggle
  Controller B      - Return / Close Menu
