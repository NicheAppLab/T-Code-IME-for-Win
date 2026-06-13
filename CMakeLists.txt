# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(TCodeIMEForWin VERSION 0.2.0.0 LANGUAGES CXX)

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
# Plain arch (no JRE suffix) for build output paths; may be updated if bundling JRE
set(TCODE_ARCH_PLAIN "${TCODE_ARCH}")

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
    src/CTCodeCandidateListUI.cpp
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
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

# -----------------------------------------------------------------------------
# TCodeProxy (C# Project)
# -----------------------------------------------------------------------------
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

add_custom_target(TCodeProxy ALL
    COMMAND ${DOTNET_EXECUTABLE} publish "${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy/TCodeProxy.csproj"
        --configuration $<CONFIG>
        --output "${CMAKE_CURRENT_BINARY_DIR}/${TCODE_ARCH}/proxy"
        --runtime win-"${TCODE_ARCH}"
        --self-contained true
        -p:PublishSingleFile=true
        -p:IncludeNativeLibrariesForSelfExtract=true
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy"
    COMMENT "Building C# TCodeProxy with dotnet publish ($<CONFIG>) for ${TCODE_ARCH}"
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
    # Save original arch for build output paths, then append JRE suffix for filenames
    set(TCODE_ARCH_PLAIN "${TCODE_ARCH}")
    set(TCODE_ARCH "${TCODE_ARCH}_jre")
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

        # Common JRE copy step for MSIX layout (used when bundling JRE)
        if(TCODE_BUNDLE_JRE)
            set(MSIX_JRE_COPY_COMMAND
                COMMAND ${CMAKE_COMMAND} -E copy_directory "${TCODE_JRE_OUTPUT_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${LAYOUT_DIR}/engine/jre"
            )
            set(MSIX_JRE_DEPENDS BuildBundledJre)
        else()
            set(MSIX_JRE_COPY_COMMAND "")
            set(MSIX_JRE_DEPENDS "")
        endif()

        if(SIGN_MSIX AND SIGNTOOL_EXECUTABLE AND CERTIFICATE_PATH AND NOT CERTIFICATE_PATH STREQUAL "")
            # Resolve certificate path to absolute (in case it's relative to source dir)
            get_filename_component(CERT_ABSOLUTE_PATH "${CERTIFICATE_PATH}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            # Build and sign MSIX with existing certificate
            if(CERTIFICATE_PASSWORD AND NOT CERTIFICATE_PASSWORD STREQUAL "")
                add_custom_target(BuildMsix
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                    ${MSIX_JRE_COPY_COMMAND}
                    COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME-${TCODE_ARCH}.msix" -v
                    COMMAND ${SIGNTOOL_EXECUTABLE} sign /f "${CERT_ABSOLUTE_PATH}" /p "${CERTIFICATE_PASSWORD}" /fd SHA256 "${MSIX_OUTPUT}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                    COMMENT "Building and signing MSIX package with winapp ($<CONFIG>) for ${TCODE_ARCH}"
                    DEPENDS ${MSIX_JRE_DEPENDS}
                )
            else()
                add_custom_target(BuildMsix
                    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                    ${MSIX_JRE_COPY_COMMAND}
                    COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME-${TCODE_ARCH}.msix" -v
                    COMMAND ${SIGNTOOL_EXECUTABLE} sign /f "${CERT_ABSOLUTE_PATH}" /fd SHA256 "${MSIX_OUTPUT}"
                    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                    COMMENT "Building and signing MSIX package with winapp ($<CONFIG>) for ${TCODE_ARCH}"
                    DEPENDS ${MSIX_JRE_DEPENDS}
                )
            endif()
        else()
            # Build MSIX without signing using winapp CLI
            add_custom_target(BuildMsix
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                ${MSIX_JRE_COPY_COMMAND}
                COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME-${TCODE_ARCH}.msix" -v
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                COMMENT "Building MSIX package using winapp CLI ($<CONFIG>) for ${TCODE_ARCH}"
                DEPENDS ${MSIX_JRE_DEPENDS}
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

        # Generate the mapping file at configure time
        file(WRITE "${MSIX_BUNDLE_MAPPING}" "[Files]\n")
        file(APPEND "${MSIX_BUNDLE_MAPPING}" "\"${MSIX_OUTPUT_DIR}/TCodeIME-x86.msix\" \"TCodeIME-x86.msix\"\n")
        file(APPEND "${MSIX_BUNDLE_MAPPING}" "\"${MSIX_OUTPUT_DIR}/TCodeIME-x64.msix\" \"TCodeIME-x64.msix\"\n")
        file(APPEND "${MSIX_BUNDLE_MAPPING}" "\"${MSIX_OUTPUT_DIR}/TCodeIME-arm64.msix\" \"TCodeIME-arm64.msix\"\n")

        add_custom_target(BuildMsixBundle
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MSIX_OUTPUT_DIR}"
            COMMAND ${WINAPP_EXECUTABLE} tool -- makeappx bundle /v /o /bv "${PROJECT_VERSION}" /f "${MSIX_BUNDLE_MAPPING}" /p "${MSIX_BUNDLE_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E remove "${MSIX_BUNDLE_MAPPING}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
            COMMENT "Building multi-architecture MSIX bundle: TCodeIME-${PROJECT_VERSION}.msixbundle"
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
    set(WINGET_INSTALLER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/installer/output")

    # Compute SHA256 for each architecture installer
    set(WINGET_SHA256_X86 "")
    set(WINGET_SHA256_X64 "")
    set(WINGET_SHA256_ARM64 "")

    set(INSTALLER_X86 "${WINGET_INSTALLER_OUTPUT_DIR}/NicheAppLab.TCodeIME_${PROJECT_VERSION}_x86.exe")
    set(INSTALLER_X64 "${WINGET_INSTALLER_OUTPUT_DIR}/NicheAppLab.TCodeIME_${PROJECT_VERSION}_x64.exe")
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

    if(EXISTS "${INSTALLER_ARM64}")
        file(SHA256 "${INSTALLER_ARM64}" WINGET_SHA256_ARM64)
        string(TOUPPER "${WINGET_SHA256_ARM64}" WINGET_SHA256_ARM64)
        message(STATUS "Winget SHA256(arm64) = ${WINGET_SHA256_ARM64}")
    else()
        message(STATUS "Winget: arm64 installer not found at ${INSTALLER_ARM64} — SHA256 will be empty")
    endif()

    # Generate each manifest from its template
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