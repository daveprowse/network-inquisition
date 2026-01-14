# Dave's Network Inquisition

**Development Version: v1.0-dev-2025-01-14**

A comprehensive GTK4-based network monitoring and diagnostic tool for Linux systems.

*Nobody expects network issues!*

## Features

- 📡 **IP Address Information** - View all network interfaces and their IPv4 addresses (auto-refresh every 30s)
- 🗺️ **IP Route Information** - Display routing table (auto-refresh every 30s)
- 📶 **PING Tool** - Test network connectivity with live output and history
- 🔍 **DIG Tool** - DNS lookup functionality with detailed results
- 📊 **Network Bandwidth Graph** - Real-time visualization of RX/TX traffic with **total bytes sent/received** (60-second rolling window)
- 💻 **Split Terminal** - Two side-by-side terminals with independent font zoom support

### Advanced Features

- **Resizable Panes** - Drag the separator between PING/DIG and the graph to adjust sizes
- **Individual Panel Maximization** - Double-click any frame to maximize/restore:
  - **IP ADDRESS INFO** - Maximizes to show only IP info and terminal
  - **IP ROUTE INFO** - Maximizes to show only route info and terminal
  - **PING** - Maximizes to show only PING and terminal
  - **DIG** - Maximizes to show only DIG and terminal
  - **Network SEND/RECEIVE** - Takes 50% of screen, terminal takes other 50%
- **Live PING Output** - See each ping response as it arrives
- **Command History** - PING and DIG results are appended with clear separators
- **Green Button Flash** - Visual feedback when GO buttons are clicked or Enter is pressed
- **Terminal Font Zoom** - Ctrl+Scroll, Ctrl+Plus, Ctrl+Minus, Ctrl+0 to reset (works independently on left and right terminals)
- **Terminal Visibility Button** - Automatically appears at bottom when terminal scrolls out of view
- **Enhanced Graph Display** - Larger font size (14pt) and total bytes transferred shown

## Requirements

### Ubuntu/Debian:

**Note for Ubuntu 24.04 (Noble) users:** If you encounter dependency version conflicts, you may need to enable the `noble-updates` repository:

```bash
# Edit your sources file
sudo nano /etc/apt/sources.list.d/ubuntu.sources
```

Add these lines at the end:
```
Types: deb
URIs: http://us.archive.ubuntu.com/ubuntu/
Suites: noble-updates
Components: main restricted
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
```

Save (Ctrl+X, Y, Enter), then update:

```bash
sudo apt update
```

**Install dependencies:**
```bash
sudo apt install build-essential libgtk-4-dev libvte-2.91-gtk4-dev pkg-config
```

### Fedora:
```bash
sudo dnf install gcc gtk4-devel vte291-gtk4-devel pkg-config
```

### Arch Linux:
```bash
sudo pacman -S base-devel gtk4 vte4 pkg-config
```

## Building

```bash
make
```

## Running

```bash
./network-inq
```

## Installation (Optional)

Install system-wide:
```bash
sudo make install
```

This installs the binary to `/usr/local/bin/` and creates a desktop entry so you can launch it from your application menu.

Uninstall:
```bash
sudo make uninstall
```

## Usage Tips

- **Adjust Layout**: Drag the horizontal line between PING/DIG and the graph
- **Maximize Graph**: Double-click anywhere on the Network SEND/RECEIVE frame
- **PING/DIG**: Press Enter in the input field or click GO
- **Terminal Zoom**: Use Ctrl+Scroll or Ctrl+Plus/Minus
- **Find Terminal**: If it scrolls out of view, click the "TERMINAL ↓" button

## Keyboard Shortcuts

- `Enter` in PING field - Execute ping
- `Enter` in DIG field - Execute dig
- `Ctrl+Scroll` - Zoom terminal font
- `Ctrl+Shift++` or `Ctrl+=` - Zoom in terminal
- `Ctrl+-` - Zoom out terminal  
- `Ctrl+0` - Reset terminal zoom

## File Structure

- `network-inq.c` - Main source code
- `Makefile` - Build configuration
- `README.md` - This file

## Technical Details

- **Language**: C
- **GUI Framework**: GTK4
- **Terminal Emulator**: VTE (libvte-2.91-gtk4)
- **Graphics**: Cairo for real-time graph rendering
- **Network Stats**: Direct /sys/class/net/ filesystem access
- **Refresh Intervals**: 30s (IP/Routes), 1s (Graph)

## Website

https://prowse.tech

## Version

This is **Version 1.0** (Development) - Pre-release version for testing and refinement.

## Author

Dave Prowse
