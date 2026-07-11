<div align="center">
<img src="https://github.com/hateweb/Wintamins/blob/main/assets/icon128.png?raw=true" alt="Wintamins Logo" width="128px">
<h1>Wintamins</h1>

<i>For healthy window management.</i>

[Advantages](#advantages) • [Features](#features) • [Overview](#overview) • [Installing](#installing) • [Configuring](#configuring) • [Building](#building) • [Roadmap](#roadmap)
</div>

**Wintamins** is a lightweight tool designed to improve window management on Windows, bringing a different way to control your workflow, similar to Linux. It serves as a love letter to Stefan Sundin's [AltDrag](https://stefansundin.github.io/altdrag), improving on the existing ideas, and being even more lightweight and optimized.

**Win**tamins. Vitamins for Windows. Win**dose of medications**. Make Windows a modern system. Modernize (Linuxify) your Windows workflow.

# Advantages
- **Blazing fast.** Runtime performance is the main goal. CPU usage is basically nonexistent.
- **Tiny footprint.** Wintamins uses around 2MB of RAM, and **there's room for improvement.**
- **Feature-rich.** Not yet, haha. [Working on it.](#roadmap)

# Features
- Do [any action](#overview) by pressing `Win` + `Mouse Button`  on any part of the window.
- Control the volume using `Win` + `Scroll Wheel`.
- Disable window titlebars (experimental).

[*And more to come...*](#roadmap)

## Overview
| Feature | Default hotkey | Note |
| --- | --- | --- |
| Drag | `Mod` + `Left Click` | Moves the window.<br>(has an option to focus a window automatically) |
| Resize | `Mod` + `Right Click` | Grabs one of the corners to resize.<br>(has options to grab the closest window corner,<br>or to snap the cursor to the corner). |
| Toggle maximize | `Mod` + `Middle Click` | Toggles the window's maximize state. |
| Minimize | Unbound | Minimizes the window. |
| Center | Unbound | Centers the window on the current monitor. |
| Bring to foreground | Unbound | Brings the window to your view. |
| Send to bottom | Unbound | Sends the window under all the other windows. |
| Toggle always on top | Unbound | Pins the window, making it float on top of every other program. |
| Close | Unbound | Closes the window. |

# Installing
> [!WARNING]
> Be aware that most web browers (and some antiviruses) false flag Wintamins as malicious software.

[Download](https://github.com/hateweb/Wintamins/releases) and run. The config file and logs are created in the same directory with the program.

# Configuring
Settings menu can be accessed by right-clicking the tray icon and choosing **Options**. Alternatively, you could edit the `Wintamins.ini` file next to the executable. However, it's not very intuitive **yet**.

You might want to take a look at [the wiki](https://github.com/hateweb/Wintamins/wiki) for something useful.

# Building
### Prerequisites
- C Compiler (`clang` is preferred)
- Resource Compiler (`windres` is preferred)
- GNU Make
- compiledb (if you want proper `clangd` support)

### How to actually build
**1.** `git clone` the repository.
```
git clone https://github.com/hateweb/Wintamins.git && cd Wintamins
```
**2.** Edit `Makefile` to match your compilers of choice. (optional)
```
nvim Makefile
```
```
  1  CC = clang
  2  RC = windres
```
**3.** Build!
```
make
```
**4.** Generate `compile_commands.json` for your LSP. (optional)
```
make compiledb
```

# Roadmap
Wintamins is a new project, is in active development stage, and will be for quite some time.

You can [go ahead and tell me what you want in this project](https://github.com/hateweb/Wintamins/issues), or [send PRs...](https://github.com/hateweb/Wintamins/pulls)

---
Thank you for reading this. Enjoy the software.
