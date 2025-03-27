# Samsung Galaxy Book Linux Control Tool

A command-line tool to control various features of Samsung Galaxy Book laptops running Linux.

## Features

- Battery charge threshold control
- Fan speed monitoring
- Performance mode control
- Recording permission control
- Keyboard backlight control
- Start on lid open control
- USB charging control

## Prerequisites

### Required Driver
This tool requires the Samsung Galaxy Book Extras driver to be installed first. Please follow the installation instructions at:
https://github.com/joshuagrisham/samsung-galaxybook-extras

### For Linux Users
- C++ compiler (g++ or clang++)
- CMake (version 3.10 or higher)
- Build tools (make, etc.)

## Building

### On Linux
```bash
mkdir build
cd build
cmake ..
make
```

## Installation

After building, you can install the tool system-wide:

```bash
# From the build directory
sudo make install
```

This will install the `samsung-cli` binary to `/usr/local/bin` by default.

## Uninstallation

To uninstall the tool:

```bash
# From the build directory
sudo make uninstall
```

## Usage

The tool requires root privileges for most operations. Use `sudo` when running commands.

```bash
# Show help
sudo samsung-cli help

# Battery charge threshold
sudo samsung-cli power read
sudo samsung-cli power set 85

# Fan speed
sudo samsung-cli fan read

# Performance mode
sudo samsung-cli perf read
sudo samsung-cli perf set balanced
sudo samsung-cli perf list

# Recording permission
sudo samsung-cli record read
sudo samsung-cli record set 1

# Keyboard backlight
sudo samsung-cli kbd read
sudo samsung-cli kbd set 3

# Start on lid open
sudo samsung-cli lid read
sudo samsung-cli lid set 1

# USB charging
sudo samsung-cli usb read
sudo samsung-cli usb set 1
```

Note: The keyboard backlight is affected by:
1. Ambient light sensor (automatically adjusts based on lighting conditions)
2. GNOME's automatic backlight control (reduces brightness after idle)
3. Manual control through this tool (values 0-3)

## License

This project is licensed under the MIT License - see the LICENSE file for details. 
