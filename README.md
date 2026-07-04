<div align="center">
<img src="https://github.com/hateweb/Wintamins/blob/main/assets/icon128.png?raw=true" alt="Wintamins Logo" width="128px">
<h1>Wintamins</h1>

<i>For healthy window management.</i>

[Advantages](#advantages) • [Features](#features) • [Overview](#overview) • [Installing](#installing) • [Configuring](#configuring) • [Building](#building) • [Roadmap](#roadmap)

<div align="left">

**Wintamins** is a lightweight tool designed to improve window management on Windows, bringing a different way to control your workflow, similar to Linux. It serves as a love letter to Stefan Sundin's [AltDrag](https://stefansundin.github.io/altdrag), improving on the existing ideas, and being even more lightweight and optimized.

**Win**tamins. Vitamins for Windows. Win**dose of medications**. Make Windows a modern system. Modernize (Linuxify) your Windows workflow.

# Advantages
- **Blazing fast.** Runtime performance is the main goal. CPU usage is basically nonexistent.
- **Tiny footprint.** Wintamins uses under 2MB of RAM, and **there's room for improvement.**
- **Feature-rich.** Not yet, haha. [Working on it.](#roadmap)

# Features
- Drag, resize, minimize, maximize or send to bottom by pressing *Win + Mouse Button*  on any part of the window.
- Hide titlebars and frames around windows (experimental).

[*And more to come...*](#roadmap)

## Overview
| Feature | Default hotkey | Note |
| --- | --- | --- |
| Drag | `Mod` + `Left Click` | Moves the window.<br>(has an option to focus a window automatically) |
| Resize | `Mod` + `Right Click` | Grabs one of the corners to resize.<br>(has options to grab the closest window corner, or to snap the cursor to the corner). |
| Toggle maximize | `Mod` + `Middle Click` | Toggles the window's maximize state. |
| Minimize | Unbound | Minimizes the window. |
| Send to bottom | Unbound | Sends the window under all the other windows. |

# Installing
[Download](https://github.com/hateweb/Wintamins/releases) and run. The config file is created where the program lives.

# Configuring
Settings menu can be accessed by right-clicking the tray icon and choosing **Options**. Alternatively, you could edit the `Wintamins.ini` file next to the executable. However, it's not very intuitive **yet**.

# Building
### Prerequisites
- C Compiler (`clang` is preferred)
- Resource Compiler (`windres` is preferred)
- GNU Make
- compiledb (if you want proper `clangd` support)

### How to actually build
**1.** `git clone` and `cd` into the build directory.
```
git clone https://github.com/hateweb/Wintamins.git
cd Wintamins\out
```
**2.** Edit `Makefile` to match your compilers of choice.
```
nvim Makefile
```
```
  1  CC = clang
  2  RC = windres
```
**3.** Uncomment the debug flags, if needed
```
  4  CFLAGS = -Wall -Wextra -g -O2 -mwindows -flto -MMD -MP
  5  # CFLAGS = -O2 -mwindows -flto -MMD -MP
```
**4.** Build!
```
make
```
**5.** Generate `compile_commands.json` for your LSP (optional)
```
make compiledb
```

# Roadmap
Wintamins is a new project, is in active development stage, and will be for quite some time.

### The following goals are prioritized.
- Toggle the program without exiting it.
- Basic window snapping.
- An auto-updater.
- A cool website.
- Scroll wheel related features.
- Improve performance even further, possibly shaving off CPU cycles.
- Whatever **you** want! [Go ahead and tell me what you want in this project!](https://github.com/hateweb/Wintamins/issues) Or [send PRs...](https://github.com/hateweb/Wintamins/pulls)
---
Thank you for reading this. 3nj0y d4 s0ftw4r3.
