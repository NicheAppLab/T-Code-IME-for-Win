# T-Code IME for Windows

This repository contains the T-Code Input Method Editor (IME) for Windows, composed of a native Text Services Framework (TSF) DLL, a C# Proxy Host, and a Java-based T-Code engine.

The entire build, compilation, and installer packaging system is unified under **CMake** with **CMakePresets.json** for multi-architecture support (x64, x86, ARM64).

---

## Prerequisites

To build and package the project, ensure you have the following installed and available on your system `PATH`:
1. **CMake** (v3.20 or newer)
2. **Visual Studio 2022** (with C++ Desktop Development tools)
3. **.NET SDK 10.0** (or newer)
4. **Inno Setup 6 or 7** — Required to compile the installer package
5. **Java Runtime Environment (JRE)** — Required by the T-Code engine
6. **winapp CLI** — Required for MSIX packaging (install via `dotnet tool install -g winapp`)

---

## Quick Start (x64)

Configure and build all artifacts for x64 in one go:

```powershell
# 1. Configure
cmake --preset x64

# 2. Build binaries + Inno Setup installer + MSIX package
cmake --build --preset artifacts-x64
```

Outputs:
- `build/x64/Release/TCodeIME.dll` — Native TSF library
- `build/x64/proxy/TCodeProxy.exe` — C# Proxy host
- `packaging/output/TCodeIME-x64.msix` — Modern MSIX package
- `installer/output/NicheAppLab.TCodeIME_0.1.1.0_x64.exe` — Traditional installer

---

## Multi-Architecture Builds

The project supports three architectures via CMake presets:

| Architecture | Configure Preset | Build Preset          | Binary Directory   |
|--------------|------------------|-----------------------|--------------------|
| x64 (64-bit) | `x64`            | `artifacts-x64`       | `build/x64/`       |
| x86 (32-bit) | `win32`          | `artifacts-win32`     | `build/win32/`     |
| ARM64        | `arm64`          | `artifacts-arm64`     | `build/arm64/`     |

### Build for a specific architecture

```powershell
cmake --preset x64
cmake --build --preset artifacts-x64
```

Replace `x64` with `win32` or `arm64` as needed.

### Build all architectures individually

```powershell
# x64
cmake --preset x64 && cmake --build --preset artifacts-x64

# x86
cmake --preset win32 && cmake --build --preset artifacts-win32

# ARM64
cmake --preset arm64 && cmake --build --preset artifacts-arm64
```

---

## MSIX Bundle (Multi-Architecture)

After building all three architectures, create a single `.msixbundle` containing x64, x86, and ARM64 packages:

```powershell
cmake --build --preset bundle-msix
```

Output: `packaging/output/TCodeIME-0.1.1.0.msixbundle`

Windows will automatically select the correct architecture at install time.

---

## Build Targets Reference

| CMake Target            | Description                                    |
|-------------------------|------------------------------------------------|
| `TCodeIME`              | Native C++ TSF DLL                             |
| `TCodeProxy`            | C# Proxy host (built automatically)            |
| `BuildMsix`             | Per-architecture MSIX package                  |
| `BuildInnoInstaller`    | Traditional Inno Setup installer               |
| `BuildMsixBundle`       | Multi-architecture MSIX bundle                 |

The `artifacts-*` build presets combine `BuildInnoInstaller` + `BuildMsix`. The `bundle-msix` preset runs `BuildMsixBundle`.

---

## How to Install & Configure

### Installing via MSIX
1. Navigate to `packaging/output/` and double-click `TCodeIME-x64.msix` (or the `.msixbundle`).
2. Sideload the package directly. Windows will automatically configure the background Startup Task to launch `TCodeProxy.exe`.
3. To install via PowerShell:
   ```powershell
   Add-AppxPackage -Path .\packaging\output\TCodeIME-x64.msix
   ```

### Installing via Win32 Setup
1. Run `installer/output/NicheAppLab.TCodeIME_0.1.1.0_x64.exe` (requires Administrator privileges).
2. Follow the wizard steps to complete registration.

### Language & JRE Setup
1. **Java Runtime Setup**: Ensure a JRE is installed and `JAVA_HOME` is defined in your environment path, as it is required by the T-Code translation engine.
2. **Add Keyboard**: Go to **Settings > Time & Language > Language & Region > Preferred Languages > Options**, and add the **T-Code IME** keyboard.

---

## System Architecture

Here is the system architecture diagram showing how the native Windows TSF DLL, the C# Proxy, the Scala Server/Engine, and the Android App components interact:

![System Architecture Diagram](https://www.plantuml.com/plantuml/png/ZLBDQiCm3BxhAOHtBEu3zDHHEXZxeLiBEummd5XDJMChEExQCVhkwvnkgJC2ErhwVZvPcZP1-R2p5RokPgm9Rn_ck6QFByPftZSPIYzHF2fB6XUc9W6NZzWzCnUU3nyP9A-MNJulPW80QbLiLKae38zB4pQxVEnCiI5LrvNAnw5WJGtvT_82Fm5_X9Unb07YVmJ8bSPHA_0hCO_5B9Qq2YfN3Q5OFjsRG08GZt24DcOtKMUSYg3ic2aGfPPznsxeWcsQzkqxhh7dhBt_ltRQiz7PATWVVoq7qU0u40QkaTSuiD89paQZ19ZixDI-Ihl9a1DS5gfIBIBd4DX6K99n03IGUe9t72hZBZvhHOSG7Kr13TvuPMdFVYk6prJBOLOd79UCbSb5_r2rl6p2b9pUsUeB)

*The raw PlantUML code is also saved in [diagram.puml](diagram.puml) for reference and easy editing.*

---

## Project Structure

- `src/` — Native C++ Text Services Framework (TSF) implementation.
- `proxy/` — C# Proxy UI Host (coordinates named pipe communication between the DLL and the engine).
- `engine/` — T-Code Java engine runtime.
- `installer/` — Inno Setup packaging configurations and installer output.
- `packaging/` — Winapp CLI packaging configuration, manifests, assets, and MSIX output.
- `CMakeLists.txt` — Unified root CMake configuration orchestrating the entire build.
- `CMakePresets.json` — Presets for multi-architecture configure, build, and bundle workflows.
