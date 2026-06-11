# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(TCodeIMEForWin VERSION 0.1.1.0 LANGUAGES CXX)

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
if(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
    set(TCODE_ARCH "x86")
    set(TCODE_ARCH_MSIX "x86")
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "ARM64")
    set(TCODE_ARCH "arm64")
    set(TCODE_ARCH_MSIX "arm64")
else()
    # Default to x64 if not specified or if platform is empty
    set(TCODE_ARCH "x64")
    set(TCODE_ARCH_MSIX "x64")
endif()

message(STATUS "Target architecture: ${TCODE_ARCH} (CMAKE_GENERATOR_PLATFORM=${CMAKE_GENERATOR_PLATFORM})")

# Set package identity and publisher for MSIX
set(PACKAGE_NAME "NicheAppLab.T-CodeIME")
set(PUBLISHER_DN "CN=2BF2953C-66FF-4938-ACE4-42FB3950D28C")
set(DISPLAY_NAME "T-Code IME")
set(PUBLISHER_DISPLAY_NAME "Niche App Lab")
set(CMAKE_CXX_STANDARD 17)
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
    src/Globals.cpp
    src/IPCClient.cpp
    src/PipeServer.cpp
    src/ProcessManager.cpp
    src/ProxyMain.cpp
    src/Registry.cpp
    src/EngineClient.cpp
    src/TCodeIME.rc
)
add_library(TCodeIME SHARED ${IME_SRC_FILES})

# Ensure C++17 features (including std::filesystem)
if (MSVC)
    target_compile_options(TCodeIME PRIVATE /std:c++17)
endif()

target_compile_features(TCodeIME PUBLIC cxx_std_17)

target_include_directories(TCodeIME PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(TCodeIME PRIVATE
    ole32
    oleaut32
    uuid
    advapi32
    imm32
)

target_compile_definitions(TCodeIME PRIVATE UNICODE _UNICODE)
set_target_properties(TCodeIME PROPERTIES
    LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/TCodeIME.def"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}"
)

# -----------------------------------------------------------------------------
# TCodeProxy (C# Project)
# -----------------------------------------------------------------------------
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

add_custom_target(TCodeProxy ALL
    COMMAND ${DOTNET_EXECUTABLE} build "${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy/TCodeProxy.csproj" --configuration $<CONFIG> --output "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/proxy"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy"
    COMMENT "Building C# TCodeProxy with dotnet build ($<CONFIG>) for ${TCODE_ARCH}"
)

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
                "/DTCodeIMEDllPath=${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/$<CONFIG>/TCodeIME.dll"
                "/DTCodeProxyBinDir=${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/proxy"
                "${CMAKE_CURRENT_SOURCE_DIR}/installer/TCodeIMEInstaller.iss"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/installer"
            DEPENDS TCodeIME TCodeProxy
            COMMENT "Building Inno Setup installer package ($<CONFIG>) for ${TCODE_ARCH}"
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# TCodeMSIX (Winapp CLI MSIX Package) - per-architecture MSIX
# -----------------------------------------------------------------------------
option(BUILD_MSIX "Build the modern MSIX package using winapp CLI" ON)

if(BUILD_MSIX)
    # 1. Read the template and generate the manifest with proper substitution
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Package.appxmanifest.in" MANIFEST_TEMPLATE)
    
    # 2. Create resource entries (each on a new line)
    set(RESOURCES_XML "")
    set(IME_RESOURCES en-us ja-jp)
    foreach(lang ${IME_RESOURCES})
        string(APPEND RESOURCES_XML "    <Resource Language=\"${lang}\" />\n")
    endforeach()
    
    # 3. Perform variable substitution (architecture placeholder too)
    string(REPLACE "@PACKAGE_NAME@" "${PACKAGE_NAME}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@PUBLISHER_DN@" "${PUBLISHER_DN}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@PROJECT_VERSION@" "${PROJECT_VERSION}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@DISPLAY_NAME@" "${DISPLAY_NAME}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@PUBLISHER_DISPLAY_NAME@" "${PUBLISHER_DISPLAY_NAME}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@RESOURCES_XML@" "${RESOURCES_XML}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@TCODE_ARCH_MSIX@" "${TCODE_ARCH_MSIX}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    
    # 4. Write the manifest and copy to layout directory
    set(MANIFEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Package.appxmanifest")
    set(MANIFEST_LAYOUT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/layout/AppxManifest.xml")
    file(WRITE "${MANIFEST_SOURCE}" "${MANIFEST_TEMPLATE}")
    configure_file("${MANIFEST_SOURCE}" "${MANIFEST_LAYOUT}" COPYONLY)

    # 5. Architecture-aware MSIX output path
    set(MSIX_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/output/TCodeIME-${TCODE_ARCH}.msix")

    # 6. Find winapp CLI and SignTool
    find_program(WINAPP_EXECUTABLE winapp REQUIRED)
    
    find_program(SIGNTOOL_EXECUTABLE signtool
        PATHS
            "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
            "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26200.0/x64"
            "C:/Program Files/Windows Kits/10/bin/x64"
        NO_DEFAULT_PATH
    )
    
    if(NOT SIGNTOOL_EXECUTABLE)
        find_program(SIGNTOOL_EXECUTABLE signtool)
    endif()
    
    if(NOT WINAPP_EXECUTABLE)
        message(WARNING "winapp CLI not found. The 'TCodeMSIX' target will not be available.")
    else()
        set(LAYOUT_DIR "layout")
        set(OUTPUT_DIR "output")
        option(SIGN_MSIX "Sign MSIX with certificate" OFF)
        set(CERTIFICATE_PATH "" CACHE STRING "Path to certificate file (.pfx)")
        set(CERTIFICATE_PASSWORD "" CACHE STRING "Password for certificate (optional)")

        if(SIGN_MSIX AND SIGNTOOL_EXECUTABLE AND CERTIFICATE_PATH AND NOT CERTIFICATE_PATH STREQUAL "")
            # Build and sign MSIX with existing certificate
            if(CERTIFICATE_PASSWORD AND NOT CERTIFICATE_PASSWORD STREQUAL "")
                add_custom_target(BuildMsix
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                    COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME-${TCODE_ARCH}.msix" -v
                    COMMAND ${SIGNTOOL_EXECUTABLE} sign /f "${CERTIFICATE_PATH}" /p "${CERTIFICATE_PASSWORD}" /fd SHA256 "${MSIX_OUTPUT}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                    COMMENT "Building and signing MSIX package with winapp ($<CONFIG>) for ${TCODE_ARCH}"
                )
            else()
                add_custom_target(BuildMsix
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                    COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME-${TCODE_ARCH}.msix" -v
                    COMMAND ${SIGNTOOL_EXECUTABLE} sign /f "${CERTIFICATE_PATH}" /fd SHA256 "${MSIX_OUTPUT}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                    COMMENT "Building and signing MSIX package with winapp ($<CONFIG>) for ${TCODE_ARCH}"
                )
            endif()
        else()
            # Build MSIX without signing using winapp CLI
            add_custom_target(BuildMsix
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME-${TCODE_ARCH}.msix" -v
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                COMMENT "Building MSIX package using winapp CLI ($<CONFIG>) for ${TCODE_ARCH}"
            )
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# BuildMsixBundle - multi-architecture MSIX bundle (x86 + x64 + arm64)
# -----------------------------------------------------------------------------
# This target creates an .msixbundle that contains MSIX packages for all three
# architectures. Prerequisites: you must first build BuildMsix for each arch:
#   cmake --build build/x64   --config Release --target BuildMsix
#   cmake --build build/win32 --config Release --target BuildMsix
#   cmake --build build/arm64 --config Release --target BuildMsix
#
# Then run this target from the x64 build directory:
#   cmake --build build/x64 --config Release --target BuildMsixBundle
option(BUILD_MSIX_BUNDLE "Build the multi-architecture MSIX bundle" ON)

if(BUILD_MSIX_BUNDLE)
    find_program(WINAPP_EXECUTABLE winapp REQUIRED)

    if(NOT WINAPP_EXECUTABLE)
        message(WARNING "winapp CLI not found. The 'BuildMsixBundle' target will not be available.")
    else()
        # Paths to pre-built per-architecture MSIX packages
        set(MSIX_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packaging/output")
        set(MSIX_BUNDLE_OUTPUT "${MSIX_OUTPUT_DIR}/TCodeIME-${PROJECT_VERSION}.msixbundle")

        # Mapping file for makeappx bundle
        set(MSIX_BUNDLE_MAPPING "${CMAKE_CURRENT_BINARY_DIR}/msixbundle_mapping.txt")

        # Generate the mapping file at build time
        add_custom_target(BuildMsixBundle
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MSIX_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E echo "[Files]" > "${MSIX_BUNDLE_MAPPING}"
            COMMAND ${CMAKE_COMMAND} -E echo "\"${MSIX_OUTPUT_DIR}/TCodeIME-x86.msix\" \"TCodeIME-x86.msix\"" >> "${MSIX_BUNDLE_MAPPING}"
            COMMAND ${CMAKE_COMMAND} -E echo "\"${MSIX_OUTPUT_DIR}/TCodeIME-x64.msix\" \"TCodeIME-x64.msix\"" >> "${MSIX_BUNDLE_MAPPING}"
            COMMAND ${CMAKE_COMMAND} -E echo "\"${MSIX_OUTPUT_DIR}/TCodeIME-arm64.msix\" \"TCodeIME-arm64.msix\"" >> "${MSIX_BUNDLE_MAPPING}"
            COMMAND ${WINAPP_EXECUTABLE} tool -- makeappx bundle /v /o /bv "${PROJECT_VERSION}" /f "${MSIX_BUNDLE_MAPPING}" /p "${MSIX_BUNDLE_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E remove "${MSIX_BUNDLE_MAPPING}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
            COMMENT "Building multi-architecture MSIX bundle: TCodeIME-${PROJECT_VERSION}.msixbundle"
        )
    endif()
endif()

# Clean up build artifacts
add_custom_target(Clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Cleaning build directory"
)