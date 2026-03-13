# Simple Camera for Advanced Filming & Photography

> [!NOTE]  
> Supports GTA 5 Legacy, FiveM, and GTA 5 Enhanced.

A comprehensive cinematic toolkit for advanced camera control and world manipulation in Grand Theft Auto V. Inspired by professional camera equipment and photography tools.

## ✨ Key Features

- **Advanced 6-Axis Free Camera**: Seamless movement with both Euler and Quaternion rotation modes.
- **FPV Drone Mode**: Physics-based flight simulation with configurable acceleration, drag, and gravity.
- **Cinematic FOV**: Features velocity-based inertia for smooth, natural zoom transitions.
- **Intelligent Tracking**: Dynamic entity tracking with visual markers and rigid mounting options.
- **Environmental Control**: Precise time-of-day adjustment (minute-by-minute), weather blending, and timelapse modes.
- **Professional Tools**: Custom Roll/Tilt settings, configurable zoom speeds, and automatic HUD/player hiding.
- **IGCS Integration**: Full support for IGCS tools and high-end Depth of Field rendering.

---

## 🚀 Installation

### Singleplayer
1.  Install [ScriptHookV](http://www.dev-c.com/gtav/scripthookv/) and an ASI Loader.
2.  Place `SimpleCamera.asi` into your main Grand Theft Auto V directory.
3.  *(Optional)* For IGCS support:
    - Install [ReShade](https://reshade.me/) with **full Add-on support**.
    - Place `IgcsConnector.addon64` into your game directory.
    - Install `IgcsDof.fx` into your `reshade-shaders/shaders` folder.
4.  *(Optional)* Edit `SimpleCamera.ini` to customize your menu toggle key (Default: **F5**).

### FiveM
1.  Copy `SimpleCamera.asi` into your `FiveM\FiveM.app\plugins` folder.
2.  *(Optional)* For IGCS support:
    - Verify ReShade is installed for FiveM with **Add-on support**.
    - Place `IgcsConnector.addon64` into your `plugins` directory.
    - Install `IgcsDof.fx` into your ReShade shaders folder.

---

## 🎮 Controls Reference

### ⌨️ Keyboard
| Action | Key(s) |
| :--- | :--- |
| **Open/Close Menu** | `F5` |
| **Move** | `W` `A` `S` `D` |
| **Ascend / Descend** | `Space` / `Ctrl` |
| **Roll Left / Right** | `Q` / `E` |
| **Look Around** | `Mouse` |
| **Zoom In / Out** | `+` / `-` |
| **Adjust Speed** | `Mouse Scroll` |

### 🎮 Controller
| Action | Input |
| :--- | :--- |
| **Move** | `Left Stick` |
| **Look Around** | `Right Stick` |
| **Move Up / Down** | `RT` / `LT` |
| **Roll Left / Right** | `RB` / `LB` |
| **Zoom In / Out** | `D-Pad Up` / `Down` |

---

## 📸 IGCS & Depth of Field Guide

1.  Activate **Free Camera** mode (`F5`).
2.  Open **ReShade** and enable the `IgcsDof` shader.
3.  In the **Addons** tab, click **"Start depth-of-field session"**.
4.  Set **Max Bokeh Size** (image will split).
5.  Use the **Magnifier** to point at your subject.
6.  Tweak **Focus Delta X** until the magnifier image is sharp and unified.
7.  Adjust quality and click **"Start Render"**.

---

## 🛠️ Build & Development
The project is organized for clean releases:
- **Project Structure**: External dependencies are stored in the `external/` directory.
- **Output**: Compiles directly to `SimpleCamera.asi`.

*Developed with ❤️ for the GTA community.*
