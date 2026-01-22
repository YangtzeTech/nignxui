# Nginx Config Editor

A graphical user interface for managing Nginx configuration files.

## Features

- Create, edit, and delete Nginx configuration files
- Syntax highlighting for Nginx config files
- Test and reload Nginx configuration
- Automatic domain management in /etc/hosts

## Building from Source

### Requirements

- CMake 3.10 or higher
- GTK4 development libraries
- GtkSourceView (optional, for syntax highlighting)
- pkg-config

### Build Debian Package

manual

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
dpkg-deb --build "package" "nginxui.deb"
```

Or use the Makefile:

```bash
make deb
```

The resulting `.deb` file will be in the parent directory.

## Installation

Install the Debian package:

```bash
sudo dpkg -i nginxui.deb
```

## Usage

Run `nginxui` from the command line or launch it from your application menu.

## License

nginxui is licensed under [GPL-3.0-or-later](LICENSE)
