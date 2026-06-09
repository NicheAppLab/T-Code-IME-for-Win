# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(TCodeIMEForWin VERSION 0.1.1.0 LANGUAGES CXX)

# Set package identity and publisher for MSIX
set(PACKAGE_NAME "NicheAppLab.T-CodeIME")
set(PUBLISHER_DN "CN=NicheAppLab")
set(DISPLAY_NAME "T-Code IME")
set(PUBLISHER_DISPLAY_NAME "Niche App Lab")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ... (rest of file unchanged) ...
# Removed duplicate TCodeMSIX target

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
set_target_properties(TCodeIME PROPERTIES LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/src/TCodeIME.def")

# -----------------------------------------------------------------------------
# TCodeProxy (C# Project)
# -----------------------------------------------------------------------------
find_program(DOTNET_EXECUTABLE dotnet REQUIRED)

add_custom_target(TCodeProxy ALL
    COMMAND ${DOTNET_EXECUTABLE} build "${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy/TCodeProxy.csproj" --configuration $<CONFIG>
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy"
    COMMENT "Building C# TCodeProxy with dotnet build ($<CONFIG>)"
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
        add_custom_target(TCodeInstaller
            COMMAND ${ISCC_EXECUTABLE}
                "/DMyAppVersion=${PROJECT_VERSION}"
                "/DConfig=$<CONFIG>"
                "/DTCodeIMEDllPath=$<TARGET_FILE:TCodeIME>"
                "/DTCodeProxyBinDir=${CMAKE_CURRENT_SOURCE_DIR}/proxy/TCodeProxy/bin/$<CONFIG>/net10.0-windows"
                "${CMAKE_CURRENT_SOURCE_DIR}/installer/TCodeIMEInstaller.iss"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/installer"
            DEPENDS TCodeIME TCodeProxy
            COMMENT "Building Inno Setup installer package ($<CONFIG>)"
        )
    endif()
endif()

# -----------------------------------------------------------------------------
# TCodeMSIX (Winapp CLI MSIX Package) - すべてをCMake側で処理
# -----------------------------------------------------------------------------
option(BUILD_MSIX "Build the modern MSIX package using winapp CLI" ON)

if(BUILD_MSIX)
    # 1. 4部構成の厳密なMSIXバージョン文字列を作成 (例: 0.1.1.0)
    set(MSIX_VERSION "${PROJECT_VERSION}")
    
    # 2. Read the template and generate the manifest with proper substitution
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Package.appxmanifest.in" MANIFEST_TEMPLATE)
    
    # 3. Create resource entries (each on a new line)
    set(RESOURCES_XML "")
    set(IME_RESOURCES en-us ja-jp)
    foreach(lang ${IME_RESOURCES})
        string(APPEND RESOURCES_XML "    <Resource Language=\"${lang}\" />\n")
    endforeach()
    
    # 4. Perform variable substitution
    string(REPLACE "@PACKAGE_NAME@" "${PACKAGE_NAME}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@PUBLISHER_DN@" "${PUBLISHER_DN}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@PROJECT_VERSION@" "${PROJECT_VERSION}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@DISPLAY_NAME@" "${DISPLAY_NAME}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@PUBLISHER_DISPLAY_NAME@" "${PUBLISHER_DISPLAY_NAME}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    string(REPLACE "@RESOURCES_XML@" "${RESOURCES_XML}" MANIFEST_TEMPLATE "${MANIFEST_TEMPLATE}")
    
    # 5. Write the manifest and copy to layout directory
    set(MANIFEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Package.appxmanifest")
    set(MANIFEST_LAYOUT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/layout/AppxManifest.xml")
    file(WRITE "${MANIFEST_SOURCE}" "${MANIFEST_TEMPLATE}")
    configure_file("${MANIFEST_SOURCE}" "${MANIFEST_LAYOUT}" COPYONLY)

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
        set(MSIX_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/packaging/output/TCodeIME.msix")
        option(SIGN_MSIX "Sign MSIX with certificate" OFF)
        option(CERTIFICATE_PATH "Path to certificate file (.pfx)" "")
        option(CERTIFICATE_PASSWORD "Password for certificate (optional)" "")

        if(SIGN_MSIX AND SIGNTOOL_EXECUTABLE AND CERTIFICATE_PATH AND NOT CERTIFICATE_PATH STREQUAL "")
            # Build and sign MSIX with existing certificate
            add_custom_target(TCodeMSIX
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME.msix" -v
                COMMAND ${SIGNTOOL_EXECUTABLE} sign /f "${CERTIFICATE_PATH}" /fd SHA256 "${MSIX_OUTPUT}"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                COMMENT "Building and signing MSIX package with winapp ($<CONFIG>)"
            )
        else()
            # Build MSIX without signing using winapp CLI
            add_custom_target(TCodeMSIX
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_SOURCE_DIR}/packaging/${OUTPUT_DIR}"
                COMMAND ${WINAPP_EXECUTABLE} pack "${LAYOUT_DIR}" --manifest "layout/AppxManifest.xml" --output "${OUTPUT_DIR}/TCodeIME.msix" -v
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
                COMMENT "Building MSIX package using winapp CLI ($<CONFIG>)"
            )
        endif()
    endif()
endif()

# Clean up build artifacts
add_custom_target(Clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Cleaning build directory"
)