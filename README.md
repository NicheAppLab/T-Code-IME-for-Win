# T-Code IME for Windows

This repository contains the T-Code Input Method Editor (IME) for Windows, composed of a native Text Services Framework (TSF) DLL, a C# Proxy Host, and a Java-based T-Code engine.

The entire build, compilation, and installer packaging system is unified under **CMake**.

---

## Prerequisites

To build and package the project, ensure you have the following installed and available on your system `PATH`:
1. **CMake** (v3.20 or newer)
2. **Visual Studio 2022** (with C++ Desktop Development tools)
3. **.NET SDK 10.0** (or newer)
4. **Inno Setup 6 or 7** (Required to compile the installer package)
5. **Java Runtime Environment (JRE)** (Required by the T-Code engine)

---

## How to Build the Project

You can compile all components and package them using standard CMake commands from your terminal.

### 1. Configure the Build
From the repository root directory, configure the CMake build files:
```powershell
cmake -B build -S .
```

### 2. Compile Binaries (DLL & Proxy)
To compile both the native C++ TSF DLL and the C# Proxy host in one command, run:
```powershell
cmake --build build --config Release
```
This compiles and outputs:
- `TCodeIME.dll` (native TSF library) in `build/Release/`
- `TCodeProxy.exe` (C# Host) in `proxy/TCodeProxy/bin/Release/net10.0-windows/`

### 3. Generate the Setup Installer
To compile the Inno Setup script and package the native DLL, the C# Proxy, and the Java engine into a single redistributable setup installer, run:
```powershell
cmake --build build --config Release --target TCodeInstaller
```
This automatically invokes the Inno Setup compiler (`ISCC.exe`) and outputs:
- `TCodeIMEInstaller.exe` inside `installer/output/`

---

## How to Install & Configure

1. Locate and run `installer/output/TCodeIMEInstaller.exe` (requires Administrator privileges).
2. **Java Runtime Setup**:
   - **Auto-Detection**: If a JRE is already installed, the installer automatically detects it and prompts you to register `JAVA_HOME` and update your system `PATH`.
   - **Auto-Install via winget**: If JRE is missing, the installer will offer to automatically install JRE using `winget`. Relaunch the installer after it completes.
   - **Manual Download**: If `winget` is skipped or fails, the installer will offer to open Oracle's official downloads site. *Note: If manually installed, you must define the `JAVA_HOME` environment variable and append `%JAVA_HOME%\bin` to your system `PATH`.*
3. Complete the setup wizard.
4. Add the keyboard in Windows: Go to **Settings > Time & Language > Language & Region > Preferred Languages > Options**, and add the **T-Code IME** keyboard.

---

## System Architecture

Here is the system architecture diagram showing how the native Windows TSF DLL, the C# Proxy, the Scala Server/Engine, and the Android App components interact:

![System Architecture Diagram](https://www.plantuml.com/plantuml/png/ZLBDQiCm3BxhAOHtBEu3zDHHEXZxeLiBEummd5XDJMChEExQCVhkwvnkgJC2ErhwVZvPcZP1-R2p5RokPgm9Rn_ck6QFByPftZSPIYzHF2fB6XUc9W6NZzWzCnUU3nyP9A-MNJulPW80QbLiLKae38zB4pQxVEnCiI5LrvNAnw5WJGtvT_82Fm5_X9Unb07YVmJ8bSPHA_0hCO_5B9Qq2YfN3Q5OFjsRG08GZt24DcOtKMUSYg3ic2aGfPPznsxeWcsQzkqxhh7dhBt_ltRQiz7PATWVVoq7qU0u40QkaTSuiD89paQZ19ZixDI-Ihl9a1DS5gfIBIBd4DX6K99n03IGUe9t72hZBZvhHOSG7Kr13TvuPMdFVYk6prJBOLOd79UCbSb5_r2rl6p2b9pUsUeB)

*The raw PlantUML code is also saved in [diagram.puml](file:///C:/Users/imuno/VSCodeProjects/T-Code-IME-for-Win/diagram.puml) for reference and easy editing.*

---

## Project Structure

- `src/` — Native C++ Text Services Framework (TSF) implementation.
- `proxy/` — C# Proxy UI Host (coordinates named pipe communication between the DLL and the engine).
- `engine/` — T-Code Java engine runtime.
- `installer/` — Inno Setup packaging configurations and output files.
- `CMakeLists.txt` — Unified root CMake configuration orchestrating the entire build.
