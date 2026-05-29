# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(TCodeIMEForWin VERSION 0.1.0 LANGUAGES CXX)

# Set package identity and publisher GUID for MSIX
set(PACKAGE_NAME "NicheAppLab.T-CodeIME")
set(PUBLISHER_GUID "2BF2953C-66FF-4938-ACE4-42FB3950D28C")
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
# TCodeMSIX (Winapp CLI MSIX Package)
# -----------------------------------------------------------------------------
option(BUILD_MSIX "Build the modern MSIX package using winapp CLI" ON)

if(BUILD_MSIX)
    find_program(POWERSHELL_EXECUTABLE powershell REQUIRED)

    if(NOT POWERSHELL_EXECUTABLE)
        message(WARNING "powershell.exe not found. The 'TCodeMSIX' target will not be available.")
    else()
        add_custom_target(TCodeMSIX
            COMMAND ${POWERSHELL_EXECUTABLE}
                -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_package.ps1"
                -Version ${PROJECT_VERSION}
                -PackageName ${PACKAGE_NAME}
                -PublisherGuid ${PUBLISHER_GUID}
                -DisplayName "${DISPLAY_NAME}"
                -PublisherDisplayName "${PUBLISHER_DISPLAY_NAME}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
            COMMENT "Building signed MSIX package using winapp CLI ($<CONFIG>)"
        )
    endif()
endif()

# Clean up build artifacts
add_custom_target(Clean
    COMMAND ${CMAKE_COMMAND} -E remove_directory build
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Cleaning build directory"
)