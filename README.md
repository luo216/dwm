# dwm - My Custom Build

This is my personal, modified build of [dwm](https://dwm.suckless.org/), a dynamic window manager for X. This version introduces significant enhancements over the original, including a scrolling tile layout and window previews.

## Key Features

*   **Scrolling Tile Layout**: Unlike the standard tiling layouts, this build implements a scrolling tile layout similar to the `niri` window manager. Windows are placed in a single, continuous vertical column that you can scroll through, preventing windows from becoming too small to be useful.
*   **Window Previews**: A major addition is the ability to see a preview of a window's content, which is not available in the vanilla dwm. This allows for easier identification and navigation between windows.
*   **Lightweight & Fast**: Retains the core philosophies of dwm, remaining extremely fast, small, and efficient.
*   **Configuration in C**: All configuration is done by editing the `config.h` header file and recompiling, ensuring the window manager remains bloat-free.

## Requirements

*   A C compiler (`gcc`, `clang`, etc.) and `make`.
*   `Xlib` header files.
*   `dmenu` is highly recommended for application launching.

## Installation

1.  **Configure the build**: Edit `config.mk` to match your local setup if necessary (e.g., `X11INC` and `X11LIB` paths).
2.  **Customize configuration**: Create your personal `config.h` from the template:
    ```sh
    cp config.def.h config.h
    ```
    Then, modify `config.h` to personalize your build (fonts, colors, keybindings, etc.).
3.  **Build and install**:
    ```sh
    sudo make clean install
    ```

## Running dwm

Add the following line to your `~/.xinitrc` file to start dwm with the `startx` command:

```sh
exec dwm
```

To connect dwm to a specific display, set the `DISPLAY` environment variable:

```sh
DISPLAY=:1 exec dwm
```

To display status info in the bar, you can pipe information to `xsetroot`. For example, in your `.xinitrc`:

```sh
while xsetroot -name "`date` `uptime | sed 's/.*,//'`"
do
	sleep 1
done &
exec dwm
```

## Keybindings

The `MODKEY` is set to `Mod4Mask` (the Super/Windows key).

### Applications & System Control

| Keybinding              | Action                               | Command/Details                            |
| ----------------------- | ------------------------------------ | ------------------------------------------ |
| `MODKEY` + `Shift` + `Enter` | Launch Terminal                      | `wezterm`                                  |
| `MODKEY` + `p`            | Launch Application Launcher        | `rofi -show`                               |
| `MODKEY` + `Shift` + `p`  | Launch Web Browser                 | `google-chrome-stable`                     |
| `MODKEY` + `e`            | Launch File Manager                | `thunar`                                   |
| `MODKEY` + `F10`          | Take Screenshot (GUI)              | `flameshot gui`                            |
| `MODKEY` + `F8` / `F9`    | Decrease / Increase Screen Brightness | `light -U 5` / `light -A 5`                |
| `MODKEY` + `F5` / `F6`    | Decrease / Increase Volume         | `pactl set-sink-volume @DEFAULT_SINK@ -5%/+5%` |
| `MODKEY` + `F3`           | Mute / Unmute Volume               | `pactl set-sink-mute @DEFAULT_SINK@ toggle` |
| `MODKEY` + `u` / `n`      | Simulate Mouse Scroll Up / Down    | `xdotool click 4` / `xdotool click 5`      |
| `MODKEY` + `Shift` + `q`  | Quit dwm                             |                                            |

### Window Management

| Keybinding              | Action                                     |
| ----------------------- | ------------------------------------------ |
| `MODKEY` + `j` / `k`      | Focus next / previous window in the stack  |
| `MODKEY` + `Shift` + `j` / `k` | Move focused window to the edge of the stack |
| `MODKEY` + `i` / `d`      | Increase / decrease number of master windows |
| `MODKEY` + `h` / `l`      | Decrease / increase master area size       |
| `MODKEY` + `Shift` + `c`  | Close focused window                     |
| `MODKEY` + `b`            | Toggle status bar visibility               |
| `MODKEY` + `Tab`          | View the previously focused tag            |
| `MODKEY` + `Alt`          | Switch focus to the previously focused window |
| `MODKEY` + `s`            | Show the most recently hidden window       |
| `MODKEY` + `Shift` + `s`  | Show all hidden windows on the current tag |
| `MODKEY` + `o`            | Show only the focused window (hides others) |
| `MODKEY` + `Shift` + `h`  | Hide the focused window                    |
| `MODKEY` + `Shift` + `f`  | Toggle fullscreen for the focused window   |
| `MODKEY` + `Shift` + `Space` | Toggle floating mode for the focused window |

### Layout Management

| Keybinding              | Action                                     |
| ----------------------- | ------------------------------------------ |
| `MODKEY` + `t`            | Set Tiling layout                          |
| `MODKEY` + `f`            | Set Floating layout                        |
| `MODKEY` + `m`            | Set Monocle layout                         |
| `MODKEY` + `Return`       | Toggle lean edge (left/right master area)  |
| `MODKEY` + `Space`        | Toggle horizontal gaps                     |

### Window Previews

| Keybinding              | Action                                     |
| ----------------------- | ------------------------------------------ |
| `MODKEY` + `Shift` + `r`  | Preview all windows on the current tag     |
| `MODKEY` + `r`            | Preview the currently focused window       |

### Tag (Workspace) Management

| Keybinding              | Action                                     |
| ----------------------- | ------------------------------------------ |
| `MODKEY` + `[1-9]`        | View tag `[1-9]`                           |
| `MODKEY` + `Shift` + `[1-9]` | Move focused window to tag `[1-9]`       |
| `MODKEY` + `Ctrl` + `[1-9]` | Add/remove tag `[1-9]` from current view |
| `MODKEY` + `Ctrl` + `Shift` + `[1-9]` | Add/remove focused window from tag `[1-9]` |
| `MODKEY` + `0`            | View all tags                              |
| `MODKEY` + `Shift` + `0`  | Move focused window to all tags (make sticky) |
| `MODKEY` + `,` / `.`      | Focus previous / next monitor              |
| `MODKEY` + `Shift` + `,` / `.` | Move focused window to previous / next monitor |
