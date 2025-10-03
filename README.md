# Free FDISK for Windows

[![License: GPL v2+](https://img.shields.io/badge/License-GPLv2+-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)  
[![Windows Compatible](https://img.shields.io/badge/Platform-Windows-lightgrey.svg)](https://www.microsoft.com)  
[![Releases](https://img.shields.io/github/v/release/MrDemonid/fdisk-windows)](https://github.com/MrDemonid/FDisk/releases/tag/v1.0.0)

> **Windows Fork:** This version is a modification of the original Free FDISK project, adapted to run on modern Windows systems. All original functionality is preserved where possible.

This is a Windows-adapted fork of **Free FDISK**, a classic tool to manage partitions on disks using the Master Boot Record (MBR) partition table. This fork allows the program to run in a modern Windows environment while retaining the original functionality wherever possible.

> **Note:** This software is originally targeted at IBM-PC compatible systems running MS-DOS-like operating systems. Some behaviors may differ when running under Windows.

## Original Project

- Original software: [Free FDISK](https://github.com/FDOS/fdisk.git)
- Original author: Brian E. Reifsnyder and The FreeDOS Project
- Original license: GNU General Public License version 2 or later (GPL-2.0+)


## This Fork

- Adapted to run under Windows (console/command-line).
- Preserves original MBR partitioning logic.
- Maintains compatibility with disks up to 2 TiB in size.


## Differences from Original Free FDISK

This Windows fork introduces the following changes compared to the original DOS-based version:

- **Removed command-line options:**
    - `/REBOOT` – not needed, changes are visible immediately.
    - `/MONO` – has no meaning under Windows.
    - `/X` – Windows does not work with INT 13.
- **Added command-line options:**
    - `/IMAGE` – attach a file as a disk image.
- **Added INI-file options:**
    - `/LANG` – specifies the language used in the program.


## Minimum Requirements

- Windows XP or later (tested on Windows XP/7/10)
- Administrator privileges recommended for raw disk access

## Important Notes

- This fork **does not support GPT disks**, which are standard on modern UEFI systems.
- Use with caution: modifying disk partitions can result in data loss. Always back up important data before running.

## License

This software is distributed under the terms of the **GNU General Public License v2 (GPL-2.0)**.

- **Original copyright:** © 1998–2025 Brian E. Reifsnyder and The FreeDOS Project
- **Windows modifications:** © 2025 Andrey Hlus

By using this software, you agree to the terms of the GPL-2.0 license.
The author of the original software and the author of this Windows fork assume no responsibility for any data loss or damage caused by its use.

## Third-Party Credits

- The original FDISK relies on the **SvarLANG** library for translations, created by Mateusz Viste.

## Releases

Download precompiled binaries for Windows from the [Releases page](https://github.com/MrDemonid/FDisk/releases/tag/v1.0.0)


## Further Documentation

- [New Command Line Syntax](./doc/USAGE.md)
- [Disk Geometry and Image Files (theory)](./doc/PK8000.md)  


- [Original Readme](doc/original/README.md)
- [Original Change Log](doc/original/CHANGES.md)
- [Original Build Instructions](doc/original/INSTALL.md)
- [Original Command Line Syntax](./doc/original/USAGE.md)

## Contributing / Issues
- **Issues:** If you encounter bugs or have suggestions, please use one of the following:
  - [GitLab Issues](https://gitlab.com/MrDemonid/FDisk/-/issues) (preferred)
  - [GitHub Issues](https://github.com/MrDemonid/FDisk/issues)
- **Contributions:** Pull requests are welcome, but please ensure that any changes do not remove original licensing information or violate GPL-2.0+ terms.
- **Safety Warning:** This program modifies disk partitions. Improper use can cause **irreversible data loss**. Always back up important data and run with caution.
- **Environment:** Administrative privileges are recommended for proper access to raw disks in Windows.  
