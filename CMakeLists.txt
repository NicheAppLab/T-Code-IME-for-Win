# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(TCodeIMEForWin VERSION 0.3.0.0 LANGUAGES CXX)

# -----------------------------------------------------------------------------
# Architecture support: x64 (64-bit), Win32 (32-bit x86), or ARM64
# -----------------------------------------------------------------------------
# Usage (via CMakePresets.json):
#   cmake --preset x64                  # 64-bit
#   cmake --preset win32                # 32-bit
#   cmake --preset arm64                # ARM64
#
# Or manually:
#   cmake -B build/x64   -A x64   -S .
#   cmake -B build/win32 -A Win32 -S .
#   cmake -B build/arm64 -A ARM64 -S .
#
# The generator platform is set via -A on the cmake command line.
# We derive a short arch tag for output directories and installer use.

# 1. Detect target architecture reliably across presets
if(CMAKE_VS_PLATFORM_NAME)
    string(TOLOWER "${CMAKE_VS_PLATFORM_NAME}" TCODE_ARCH_RAW)
else()
    string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" TCODE_ARCH_RAW)
endif()

# 2. Normalize architecture identifiers
if(TCODE_ARCH_RAW STREQUAL "win32" OR TCODE_ARCH_RAW STREQUAL "x86")
    set(TCODE_ARCH "x86")
    set(TCODE_ARCH_MSIX "x86")
elseif(TCODE_ARCH_RAW STREQUAL "arm64")
    set(TCODE_ARCH "arm64")
    set(TCODE_ARCH_MSIX "arm64")
else()
    set(TCODE_ARCH "x64")
    set(TCODE_ARCH_MSIX "x64")
endif()

# 3. Synchronize internal path mapping variable
set(TCODE_ARCH_PLAIN "${TCODE_ARCH}")

# 4. Handle JRE filename suffix condition
if(TCODE_BUNDLE_JRE)
    set(INNO_JRE_SUFFIX "_jre")
else()
    set(INNO_JRE_SUFFIX "")
endif()

message(STATUS "Target architecture: ${TCODE_ARCH} (CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM})")

# -----------------------------------------------------------------------------
# Java Runtime bundling option (x64-java-runtime configuration)
# -----------------------------------------------------------------------------
# When enabled, jlink creates a minimal JRE from the x64 JDK and bundles it
# with the deployment. The tcodeserver.bat script will use the bundled JRE
# via the BUNDLED_JVM variable.
option(TCODE_BUNDLE_JRE "Bundle a jlink'd Java runtime with the deployment" OFF)

if(TCODE_BUNDLE_JRE)
    # Path to the x64 JDK for jlink
    set(TCODE_JDK_HOME "C:/Program Files/Microsoft/jdk-25.0.3.9-hotspot" CACHE PATH "Path to the JDK home for jlink")
    # Modules to include in the jlink'd JRE (customize via CMakePresets cacheVariables)
    if(NOT TCODE_JLINK_MODULES)
        set(TCODE_JLINK_MODULES "java.base,java.logging,java.sql,java.naming,java.management,java.xml,jdk.unsupported,jdk.zipfs,jdk.charsets,java.net.http,java.scripting,java.instrument,java.security.jgss,java.security.sasl,java.datatransfer,java.desktop,jdk.crypto.ec,jdk.crypto.cryptoki,jdk.crypto.mscapi,jdk.localedata,jdk.management,jdk.management.agent,jdk.naming.dns,jdk.naming.rmi,jdk.sctp,jdk.security.auth,jdk.security.jgss")
    endif()
    set(TCODE_JRE_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/jre")
    message(STATUS "Java runtime bundling ENABLED")
    message(STATUS "  JDK home: ${TCODE_JDK_HOME}")
    message(STATUS "  JRE output: ${TCODE_JRE_OUTPUT_DIR}")
    message(STATUS "  jlink modules: ${TCODE_JLINK_MODULES}")
endif()

# Set package identity and publisher for MSIX
set(PACKAGE_NAME "NicheAppLab.T-CodeIME")
set(PUBLISHER_DN "CN=2BF2953C-66FF-4938-ACE4-42FB3950D28C")
set(DISPLAY_NAME "T-Code IME")
set(PUBLISHER_DISPLAY_NAME "Niche App Lab")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Avoid building tests or examples for dependencies if any leak through
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# -----------------------------------------------------------------------------
# TCodeIME (TSF DLL)
# -----------------------------------------------------------------------------
set(IME_SRC_FILES
    src/TCodeIME.cpp
    src/dllmain.cpp
    src/ClassFactory.cpp
    src/Compartment.cpp
    src/CompartmentEventSink.cpp
    src/CTCodeModeButton.cpp
    src/CTCodeCandidateListUI.cpp
    src/Globals.cpp
    src/Registry.cpp
    src/EngineClient.cpp
    src/TCodeIME.rc
    src/EditSession.cpp
    src/GlazeModels.hpp
    src/GlazeClient.hpp
)
add_library(TCodeIME SHARED ${IME_SRC_FILES})

# Ensure C++20 features (including std::filesystem)
if (MSVC)
    target_compile_options(TCodeIME PRIVATE /std:c++latest)
    target_compile_options(TCodeIME PRIVATE /Zc:preprocessor)
endif()

if(WIN32)
    target_link_libraries(TCodeIME PRIVATE ws2_32)
endif()

find_package(asio CONFIG REQUIRED)

target_compile_features(TCodeIME PUBLIC cxx_std_20)

target_include_directories(TCodeIME PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/glaze/include"
)

target_link_libraries(TCodeIME PRIVATE
    ole32
    oleaut32
    uuid
    advapi32
    imm32
    asio::asio
)

target_compile_definitions(TCodeIME PRIVATE UNICODE _UNICODE)
set_target_properties(TCodeIME PROPERTIES
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/TCodeIME.def"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# -----------------------------------------------------------------------------
# TCodeProxy (C++ Native)
# -----------------------------------------------------------------------------
add_executable(TCodeProxy WIN32
    proxy/TCodeProxy/proxy.cpp
    proxy/TCodeProxy/resource.rc
)

set_target_properties(TCodeProxy PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/proxy"
    OUTPUT_NAME "TCodeProxy"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# -----------------------------------------------------------------------------
# Bundled JRE (jlink) - only when TCODE_BUNDLE_JRE is ON
# -----------------------------------------------------------------------------
if(TCODE_BUNDLE_JRE)
    find_program(JLINK_EXECUTABLE jlink
        PATHS
            "${TCODE_JDK_HOME}/bin"
            "C:/Program Files/Microsoft/jdk-25.0.3.9-hotspot/bin"
            "C:/Program Files/Java/jdk-25/bin"
            "C:/Program Files/Eclipse Adoptium/jdk-25.0.3.9-hotspot/bin"
        REQUIRED
    )

    # Custom target to run jlink and stage the engine + JRE for deployment
    add_custom_target(BuildBundledJre ALL
        COMMAND ${CMAKE_COMMAND} -E echo "Running jlink to create bundled JRE..."
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${TCODE_JRE_OUTPUT_DIR}"
        COMMAND ${JLINK_EXECUTABLE}
            --add-modules "${TCODE_JLINK_MODULES}"
            --output "${TCODE_JRE_OUTPUT_DIR}"
            --strip-debug
            --no-man-pages
            --no-header-files
            --compress zip-6
        COMMAND ${CMAKE_COMMAND} -E echo "jlink complete. Bundled JRE at ${TCODE_JRE_OUTPUT_DIR}"
        # Stage engine files for deployment (bin + lib) — original tcodeserver.bat
        # already supports BUNDLED_JVM, and TCodeProxy.exe sets the env vars.
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/engine/bin" "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/engine/bin"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/engine/lib" "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/engine/lib"
        WORKING_DIRECTORY "${TCODE_JDK_HOME}/bin"
        COMMENT "Creating bundled JRE with jlink for ${TCODE_ARCH}"
        DEPENDS TCodeProxy
    )

    # Make the installer and MSIX targets depend on the bundled JRE
    set(TCODE_JRE_DEPENDS BuildBundledJre)
    set(TCODE_BUNDLE_JRE_DEFINE "/DTCODEBundleJre=1")
    set(TCODE_JRE_DIR "${TCODE_JRE_OUTPUT_DIR}")
    set(TCODE_ENGINE_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/engine")
    set(TCODE_ARCH_PLAIN "${TCODE_ARCH}")
else()
    set(TCODE_JRE_DEPENDS "")
    set(TCODE_BUNDLE_JRE_DEFINE "")
    set(TCODE_JRE_DIR "")
    set(TCODE_ENGINE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/engine")
endif()

# -----------------------------------------------------------------------------
# TCodeInstaller (Inno Setup Installer)
# -----------------------------------------------------------------------------
option(BUILD_INSTALLER "Build the Inno Setup installer package" ON)
if(BUILD_INSTALLER)
    find_program(ISCC_EXECUTABLE ISCC
        PATHS
            "C:/Program Files (x86)/Inno Setup 6"
            "C:/Program Files (x86)/Inno Setup 7"
            "C:/Program Files/Inno Setup 6"
            "C:/Program Files/Inno Setup 7"
            "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
            "$ENV{LOCALAPPDATA}/Programs/Inno Setup 7"
        DOC "Inno Setup Compiler (ISCC.exe)"
    )

    if(NOT ISCC_EXECUTABLE)
        message(WARNING "ISCC.exe (Inno Setup) not found. The 'TCodeInstaller' target will not be available.")
    else()
        add_custom_target(BuildInnoInstaller
            COMMAND ${ISCC_EXECUTABLE}
                "/DMyAppVersion=${PROJECT_VERSION}"
                "/DConfig=$<CONFIG>"
                "/DTCodeArch=${TCODE_ARCH}"
                "/DTCodeIMEDllPath=${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH_PLAIN}/$<CONFIG>/TCodeIME.dll"
                "/DTCodeProxyBinDir=${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH_PLAIN}/proxy"
                ${TCODE_BUNDLE_JRE_DEFINE}
                "/DJreSuffix=${INNO_JRE_SUFFIX}"
                "/DTCODE_JRE_DIR=${TCODE_JRE_DIR}"
                "/DTCodeEngineDir=${TCODE_ENGINE_DIR}"
                "${CMAKE_CURRENT_SOURCE_DIR}/installer/TCodeIMEInstaller.iss"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/installer"
            DEPENDS TCodeIME TCodeProxy ${TCODE_JRE_DEPENDS}
            COMMENT "Building Inno Setup installer package ($<CONFIG>) for ${TCODE_ARCH}"
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# GenerateWingetManifests - generate winget-pkgs manifest YAML files from templates
# -----------------------------------------------------------------------------
# This generates winget-pkgs manifest YAML files at configure time by reading
# .in templates and substituting @PLACEHOLDERS@. SHA256 is auto-calculated
# from the built installer files in installer/output/.
#
# The installer URL is auto-generated from PROJECT_VERSION and architecture:
#   https://github.com/NicheAppLab/T-Code-IME-for-Win/releases/download/{version}/
#   NicheAppLab.TCodeIME_{version}_{arch}.exe
#
# All three architectures (x86, x64, arm64) are included in a single manifest.
#
# Workflow:
#   1. Build all three architectures:
#      cmake --build --preset artifacts-x64
#      cmake --build --preset artifacts-win32
#      cmake --build --preset artifacts-arm64
#   2. cmake --build build/x64 --target GenerateWingetManifests
#      (re-runs cmake configure to regenerate manifests)
#   3. Upload artifacts to GitHub release
#   4. Commit generated manifests to winget-pkgs repository
# -----------------------------------------------------------------------------
option(BUILD_WINGET_MANIFESTS "Build the winget-pkgs manifest files" ON)

if(BUILD_WINGET_MANIFESTS)
    set(WINGET_VERSION_DIR "${CMAKE_CURRENT_SOURCE_DIR}/winget/manifests/n/NicheAppLab/T-CodeIME/${PROJECT_VERSION}")
    set(WINGET_VERSION_DIR_JRE "${CMAKE_CURRENT_SOURCE_DIR}/winget/manifests/n/NicheAppLab/T-CodeIME-JRE/${PROJECT_VERSION}")
    set(WINGET_INSTALLER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/installer/output")

    # Compute SHA256 for each architecture installer
    set(WINGET_SHA256_X86 "")
    set(WINGET_SHA256_X64 "")
    set(WINGET_SHA256_X64_JRE "")
    set(WINGET_SHA256_ARM64 "")

    set(INSTALLER_X86 "${WINGET_INSTALLER_OUTPUT_DIR}/NicheAppLab.TCodeIME_${PROJECT_VERSION}_x86.exe")
    set(INSTALLER_X64 "${WINGET_INSTALLER_OUTPUT_DIR}/NicheAppLab.TCodeIME_${PROJECT_VERSION}_x64.exe")
    set(INSTALLER_X64_JRE "${WINGET_INSTALLER_OUTPUT_DIR}/NicheAppLab.TCodeIME_${PROJECT_VERSION}_x64_jre.exe")
    set(INSTALLER_ARM64 "${WINGET_INSTALLER_OUTPUT_DIR}/NicheAppLab.TCodeIME_${PROJECT_VERSION}_arm64.exe")

    if(EXISTS "${INSTALLER_X86}")
        file(SHA256 "${INSTALLER_X86}" WINGET_SHA256_X86)
        string(TOUPPER "${WINGET_SHA256_X86}" WINGET_SHA256_X86)
        message(STATUS "Winget SHA256(x86) = ${WINGET_SHA256_X86}")
    else()
        message(STATUS "Winget: x86 installer not found at ${INSTALLER_X86} — SHA256 will be empty")
    endif()

    if(EXISTS "${INSTALLER_X64}")
        file(SHA256 "${INSTALLER_X64}" WINGET_SHA256_X64)
        string(TOUPPER "${WINGET_SHA256_X64}" WINGET_SHA256_X64)
        message(STATUS "Winget SHA256(x64) = ${WINGET_SHA256_X64}")
    else()
        message(STATUS "Winget: x64 installer not found at ${INSTALLER_X64} — SHA256 will be empty")
    endif()

    if(EXISTS "${INSTALLER_X64_JRE}")
        file(SHA256 "${INSTALLER_X64_JRE}" WINGET_SHA256_X64_JRE)
        string(TOUPPER "${WINGET_SHA256_X64_JRE}" WINGET_SHA256_X64_JRE)
        message(STATUS "Winget SHA256(x64_jre) = ${WINGET_SHA256_X64_JRE}")
    else()
        message(STATUS "Winget: x64_jre installer not found at ${INSTALLER_X64_JRE} — SHA256 will be empty")
    endif()

    if(EXISTS "${INSTALLER_ARM64}")
        file(SHA256 "${INSTALLER_ARM64}" WINGET_SHA256_ARM64)
        string(TOUPPER "${WINGET_SHA256_ARM64}" WINGET_SHA256_ARM64)
        message(STATUS "Winget SHA256(arm64) = ${WINGET_SHA256_ARM64}")
    else()
        message(STATUS "Winget: arm64 installer not found at ${INSTALLER_ARM64} — SHA256 will be empty")
    endif()

    # Generate each manifest from its template (T-CodeIME)
    set(WINGET_TEMPLATES
        "${WINGET_VERSION_DIR}/NicheAppLab.T-CodeIME.installer.yaml.in"
        "${WINGET_VERSION_DIR}/NicheAppLab.T-CodeIME.locale.ja-JP.yaml.in"
        "${WINGET_VERSION_DIR}/NicheAppLab.T-CodeIME.yaml.in"
    )
    set(WINGET_OUTPUTS
        "${WINGET_VERSION_DIR}/NicheAppLab.T-CodeIME.installer.yaml"
        "${WINGET_VERSION_DIR}/NicheAppLab.T-CodeIME.locale.ja-JP.yaml"
        "${WINGET_VERSION_DIR}/NicheAppLab.T-CodeIME.yaml"
    )

    foreach(idx RANGE 2)
        list(GET WINGET_TEMPLATES ${idx} TEMPLATE_FILE)
        list(GET WINGET_OUTPUTS ${idx} OUTPUT_FILE)
        file(READ "${TEMPLATE_FILE}" CONTENT)
        string(REPLACE "@PROJECT_VERSION@" "${PROJECT_VERSION}" CONTENT "${CONTENT}")
        string(REPLACE "@WINGET_SHA256_X86@" "${WINGET_SHA256_X86}" CONTENT "${CONTENT}")
        string(REPLACE "@WINGET_SHA256_X64@" "${WINGET_SHA256_X64}" CONTENT "${CONTENT}")
        string(REPLACE "@WINGET_SHA256_ARM64@" "${WINGET_SHA256_ARM64}" CONTENT "${CONTENT}")
        file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
        message(STATUS "Generated winget manifest: ${OUTPUT_FILE}")
    endforeach()

    # Generate each manifest from its template (T-CodeIME-JRE)
    set(WINGET_TEMPLATES_JRE
        "${WINGET_VERSION_DIR_JRE}/NicheAppLab.T-CodeIME-JRE.installer.yaml.in"
        "${WINGET_VERSION_DIR_JRE}/NicheAppLab.T-CodeIME-JRE.locale.ja-JP.yaml.in"
        "${WINGET_VERSION_DIR_JRE}/NicheAppLab.T-CodeIME-JRE.yaml.in"
    )
    set(WINGET_OUTPUTS_JRE
        "${WINGET_VERSION_DIR_JRE}/NicheAppLab.T-CodeIME-JRE.installer.yaml"
        "${WINGET_VERSION_DIR_JRE}/NicheAppLab.T-CodeIME-JRE.locale.ja-JP.yaml"
        "${WINGET_VERSION_DIR_JRE}/NicheAppLab.T-CodeIME-JRE.yaml"
    )

    foreach(idx RANGE 2)
        list(GET WINGET_TEMPLATES_JRE ${idx} TEMPLATE_FILE)
        list(GET WINGET_OUTPUTS_JRE ${idx} OUTPUT_FILE)
        file(READ "${TEMPLATE_FILE}" CONTENT)
        string(REPLACE "@PROJECT_VERSION@" "${PROJECT_VERSION}" CONTENT "${CONTENT}")
        string(REPLACE "@WINGET_SHA256_X64_JRE@" "${WINGET_SHA256_X64_JRE}" CONTENT "${CONTENT}")
        file(WRITE "${OUTPUT_FILE}" "${CONTENT}")
        message(STATUS "Generated winget manifest: ${OUTPUT_FILE}")
    endforeach()

    # Custom target that re-runs cmake configure to regenerate manifests
    add_custom_target(GenerateWingetManifests
        COMMAND ${CMAKE_COMMAND} "${CMAKE_CURRENT_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Re-running cmake configure to regenerate winget-pkgs manifest files"
    )
endif()

# Clean up build artifacts
add_custom_target(Clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Cleaning build directory"
)