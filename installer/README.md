# T-Code IME Installer

This folder contains an Inno Setup installer script for packaging the native IME DLL, the C# proxy, and the engine directory.

## What it installs
- `TCodeIME.dll` from `build\Release`
- `TCodeProxy.exe` and runtime outputs from `proxy\TCodeProxy\bin\Release\net10.0-windows`
- `engine\` directory from the repo root
- Registers the IME DLL with `regsvr32`
- Creates a Start Menu shortcut for the proxy

## Build steps

1. Build the native IME:

```powershell
cd c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

2. Restore NuGet packages and build the proxy:

```powershell
cd c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\proxy\TCodeProxy
nuget restore TCodeProxy.csproj
dotnet build -c Release
```

> If you prefer, use `dotnet restore` instead of `nuget restore`.

3. Build the installer:

```powershell
cd c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\installer
.\build_installer.ps1
```

The script writes compiler output to `build_installer.log` in the same folder.

4. Or run the combined super script:

```powershell
cd c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\installer
.\build_all.ps1
```

Optional flags:

```powershell
.\build_all.ps1 -PackNuGet
.\build_all.ps1 -SkipInstaller
```

## NuGet notes

- `TCodeProxy.csproj` already uses `PackageReference` for gRPC and Protobuf.
- Use `nuget restore proxy\TCodeProxy\TCodeProxy.csproj` or `dotnet restore proxy\TCodeProxy\TCodeProxy.csproj` before building.
- To create a NuGet package for the proxy project, run:

```powershell
dotnet pack proxy\TCodeProxy\TCodeProxy.csproj -c Release
```

That will generate a `.nupkg` in `proxy\bin\Release`.

### NuGet package via `.nuspec`

A package manifest is available at `proxy\TCodeProxy\TCodeProxy.nuspec`.
After building the proxy in Release mode, run:

```powershell
cd c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win\proxy
.\build_nuget.ps1
```

The package will be created in `proxy\nupkg` and now contains both the native `TCodeIME.dll` and the `TCodeProxy` executable bundle.

### Private local NuGet feed for testing

This repository includes a local NuGet source registration in `nuget.config`:

- Source name: `TCodeProxyLocal`
- Source path: `proxy/nupkg`

Use this local package source to test the proxy package without publishing it publicly.

After creating the package, you can restore or install from the local feed with:

```powershell
dotnet nuget add source .\proxy\nupkg -n TCodeProxyLocal
dotnet restore --source TCodeProxyLocal
```

Or reference the local source directly in `nuget.config` from the repo root and use standard restore commands.

## Requirements

- Windows with administrator privileges for installer execution
- Inno Setup 6 installed for `build_installer.ps1`
- .NET SDK installed for building the proxy
- Java Runtime Environment installed for the engine batch script

## Java prerequisite

The installer checks for a Java runtime before proceeding. If Java is missing, install a compatible JRE using NuGet. For example:

```powershell
cd c:\Users\imuno\VSCodeProjects\T-Code-IME-for-Win
nuget install Microsoft.Java.OpenJDK.17 -OutputDirectory .\jre
```

The installer also detects a local JRE folder named `jre`, `jre-openjdk`, or `java-runtime` alongside the installer or in parent directories. It can register `JAVA_HOME` and add Java `bin` to `PATH` when you approve it.

The installer will detect an existing JRE and ask whether it should register `JAVA_HOME` and add the Java `bin` folder to `PATH`.
