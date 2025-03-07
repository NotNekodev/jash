<div align="center">

# Jash

**Just Another SHell - A lightweight, customizable Unix shell**

![GitHub License](https://img.shields.io/github/license/notnekodev/jash?style=flat-square)
![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/NotNekodev/jash/build.yml?branch=trunk&style=flat-square&logo=githubactions)
![GitHub last commit](https://img.shields.io/github/last-commit/notnekodev/jash?style=flat-square&logo=github)
![GitHub commit activity](https://img.shields.io/github/commit-activity/t/notnekodev/jash?style=flat-square&logo=git)
![GitHub Sponsors](https://img.shields.io/github/sponsors/notnekodev?style=flat-square&color=violet)


</div>

Jash is a modern Unix shell designed for simplicity, performance and extensibility. It provides a familiar command-line interface while maintaining minimal dependencies and a small footprint.

## Table of Contents

- [Overview](#overview)
- [System Requirements](#system-requirements)
- [Installation](#installation)
  - [Building from Source](#building-from-source)
  - [Setting as Default Shell](#setting-as-default-shell)
- [Features](#features)
- [Configuration](#configuration)
- [Development](#development)
- [License](#license)

## Overview

Jash (Just Another SHell) provides the essential functionality of Unix shells like Bash or Zsh while maintaining a clean, efficient codebase. It features command history, customizable prompts, and standard path resolution while using minimal dependencies.

## System Requirements

### Operating System Compatibility

| Operating System | Status |
|:----------------:|:------:|
| Linux            | ✅     |
| WSL2             | ✅     |
| Unix             | ⚠️     |
| macOS            | ⚠️     |
| Windows          | ❌     |

✅ Fully supported and tested  
⚠️ Should work but not extensively tested  
❌ Not supported

### Dependencies

- `libreadline-dev` - For input handling and command history functionality
- C compiler (`clang` or `gcc`) - For building the project
- `git` - For cloning the repository (optional)

## Installation

### Building from Source

1. Install required dependencies:

```sh
# Debian/Ubuntu
sudo apt install libreadline-dev clang git
   
# Fedora/RHEL
sudo dnf install readline-devel clang git
   
# Arch Linux
sudo pacman -S readline clang git
```

2. Clone the repository:
```sh
git clone --depth 1 https://github.com/NotNekodev/jash.git
cd jash
```

3. Build and install
```sh
make
sudo make install
```

This compiles Jash and installs it to `/usr/bin/jash`. The installation also adds Jash to you `/etc/shells` file.

### Setting as Default Shell

To set Jash as your default login shell:
```sh
chsh -s /usr/bin/jash
```

Log out and log back in for the changes to take effect.

## Features
- Clean, minimal interface
- Command history with persistence
- Standard  path resolution
- Configurable via INI file
- Extensible command completion

## Configuration
Jash can be configured by creating or editing the .`jashconf.ini`file in your home directory.

### Available Configuration Options

`[Core]`** section**

- `Prompt`: Customizes the shell prompt with the following placeholders:
    - `$$DIR$$`: Current working directory
    - `$$USER$$`: Current username
    - `$$HOST$$`: Current hostname

- `HistorySize`: The size of the command history before looping
- `HistoryFile`: Where to store the command history. (Don't use '~')
- `DefaultDirectory`: The default directory thats open when you launch the shell
- `Debug`: Enables various debug options

`[Cursor]`** section**

- `Style`: The cursor style. There are 4 possible options:
  - `block`: Block Cursor, the default one
  - `underline`: Underline Cursor
  - `bar`: A simple bar cursor
  - `custom`: uses the `CustomSequence` below to "print" the cursor configuration
- `BlinkEnables`: When enabled the cursor will blink. Otherwise not
- `BlinkInterval`: The interval in which the cursor will blink
- `CustomSequence`: What to print when the cursor configuration is `custom` (experts only)

### Example Configuration
```ini
; Prompt variables:
;  - $$USER$$ - current user name
;  - $$HOST$$ - current host name
;  - $$DIR$$ - current directory

[Core]
Prompt=$$USER$$@$$HOST$$:$$DIR$$$
HistorySize=2048
HistoryFile=/home/neko/.jash_history
DefaultDirectory=~
Debug=false

[Cursor]
; Cursor style: block, underline, bar, custom
Style=block
BlinkEnabled=true
BlinkInterval=500

; Custom cursor settings (used when Style=custom)
CustomSequence=\033[3 q
```

## Development

Jash is structured with a clean separation between core functionality components:

- Input handling with readline integration
- Command execution and process management
- Configuration parsing with INI format
- Command history management

To contribute to Jash, please fork the repository and submit pull requests with your changes.

## License
Jash is licensed under the MIT License. See [LICENSE](LICENSE) for details.

Copyright © 2025 NotNekodev