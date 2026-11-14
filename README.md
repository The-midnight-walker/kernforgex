# KernForgeX (`kfgx`)

[![License: GPL v2](https://img.shields.io/badge/License-GPL_v2-blue.svg)](LICENSE) ![Standard: C11](https://img.shields.io/badge/Standard-C11-00599C.svg?logo=c&logoColor=white) ![Platform: Linux](https://img.shields.io/badge/Platform-Linux-FCC624.svg?logo=linux&logoColor=black) ![Build: CMake](https://img.shields.io/badge/Build-CMake-064F8C.svg?logo=cmake&logoColor=white) ![Shell: POSIX](https://img.shields.io/badge/Shell-POSIX-4EAA25.svg?logo=gnu-bash&logoColor=white)

**KernForgeX** is a lightweight CLI utility designed to prepare, configure, and maintain Linux environments for kernel development, device driver programming, and low-level system engineering.

It combines a modular **C11 dispatcher** (`kfgx`) with portable **POSIX shell scripts**, driven by a central configuration file.

---

## Features

- **Package Management by Domain**: Check, install, and remove package sets for building, debugging, and system analysis.
- **Kernel Parameter Tuning**: Inspect and apply runtime kernel parameters via `/proc/sys/kernel/` (e.g., `panic_on_oops`, `sysrq`, panic reboot delay).
- **Configuration-Driven**: All packages and system parameters are defined in a single file (`kernforgex.conf`).
- **Clean Logging & Formatting**: Built-in CLI and file logging (`kfgx.log`), colored terminal output, and script linting.

---

## Project Structure

```text
├── CMakeLists.txt
├── kernforgex/
│   ├── configs/
│   │   └── kernforgex.conf
│   └── scripts/
│       ├── kernbuild.sh
│       ├── kerndebug.sh
│       ├── kernsys.sh
│       └── utils.sh
├── include/
└── src/
```

---

## Requirements

- **OS**: Linux (Debian/Ubuntu-based distributions for `apt`/`dpkg` package actions)
- **Compiler**: GCC or Clang (C11 support)
- **Build tool**: CMake (>= 3.10)
- **Shell**: POSIX-compliant `/bin/sh`
- *(Optional)*: `clang-format`, `shfmt`, `shellcheck`, `doxygen`

---

## Build

```bash
# Configure the build directory
cmake -B build

# Build the kfgx binary
cmake --build build
```

The compiled binary will be located at `build/src/kfgx`.

---

## Usage

```bash
./build/src/kfgx [subcommand] [options]
```

> **Note:** Actions that install/remove packages or write to `/proc/sys/kernel/` require root privileges (`sudo`).

### 1. Kernel Build Environment (`kbuild`)

Manage packages needed to compile Linux kernels and modules (GCC, Make, Bison, Flex, kernel headers, etc.):

```bash
# List status of build packages
./build/src/kfgx kbuild -p -l

# Install missing build packages
sudo ./build/src/kfgx kbuild -p -i

# Remove build packages
sudo ./build/src/kfgx kbuild -p -r
```

### 2. Kernel Debugging Tools (`kdbg`)

Manage debugging, tracing, and static analysis tools (GDB, Perf, trace-cmd, eBPF tools, Coccinelle, Valgrind, etc.):

```bash
# List status of debug packages
./build/src/kfgx kdbg -p -l

# Install missing debug packages
sudo ./build/src/kfgx kdbg -p -i
```

### 3. System Environment & Kernel Tuning (`ksys`)

Manage system utilities (numactl, hwloc, dwarves, etc.) and tune runtime kernel parameters:

```bash
# List system packages
./build/src/kfgx ksys -p -l

# List current vs configured kernel parameters (/proc/sys/kernel/)
./build/src/kfgx ksys -f -l

# Apply kernel parameters from configuration
sudo ./build/src/kfgx ksys -f -s
```

### Common Flags

| Flag | Long Flag | Description |
| :--- | :--- | :--- |
| `-h` | `--help` | Show command usage |
| `-v` | `--verbose` | Enable verbose output |
| `-c` | `--color` | Enable ANSI colored output |
| `-p` | `--packages` | Target package management |
| `-f` | `--files` | Target system parameter files (`ksys` only) |
| `-l` | `--list` | List items and current status |
| `-i` | `--install` | Install missing packages |
| `-r` | `--remove` | Remove installed packages |
| `-s` | `--set` | Apply settings to files (`ksys` only) |

---

## Configuration

Edit [`kernforgex/configs/kernforgex.conf`](kernforgex/configs/kernforgex.conf) to customize your setup:

- `[packages.build]`: Packages installed by `kbuild`
- `[packages.debug]`: Packages installed by `kdbg`
- `[packages.system]`: Packages installed by `ksys`
- `[system.files]`: Kernel parameters applied by `ksys -f -s` (e.g. `panic_on_oops`, `sysrq_mask`)

---

## Development

Format code and check script quality:

```bash
# Format C code and shell scripts
cmake --build build --target format

# Lint shell scripts (shellcheck)
cmake --build build --target lint-shell

# Generate Doxygen documentation (if Doxygen is installed)
cmake --build build --target doc
```

---

## License

This project is licensed under the **GNU General Public License v2.0** (GPL-2.0). See [LICENSE](LICENSE) for details.
