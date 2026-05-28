cmake_minimum_required(VERSION 3.20)
project(TCodeIMEForWin VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Avoid building tests or examples for dependencies if any leak through
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

# gRPC/Protobuf are now handled by the C# Proxy
# find_package(protobuf CONFIG REQUIRED)
# find_package(gRPC CONFIG REQUIRED)

# The generated code will go into the build directory
# set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
# file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILE "${CMAKE_CURRENT_SOURCE_DIR}/proto/tcode_engine.proto")

# Create a target to compile the proto file
# add_custom_command(
#     OUTPUT "${PROTO_GEN_DIR}/tcode_engine.pb.h"
#            "${PROTO_GEN_DIR}/tcode_engine.pb.cc"
#            "${PROTO_GEN_DIR}/tcode_engine.grpc.pb.h"
#            "${PROTO_GEN_DIR}/tcode_engine.grpc.pb.cc"
#     COMMAND protobuf::protoc
#     ARGS --proto_path="${CMAKE_CURRENT_SOURCE_DIR}/proto"
#          --cpp_out="${PROTO_GEN_DIR}"
#          --grpc_out="${PROTO_GEN_DIR}"
#          --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin>
#          "${PROTO_FILE}"
#     DEPENDS "${PROTO_FILE}" protobuf::protoc gRPC::grpc_cpp_plugin
#     COMMENT "Generating C++ and gRPC files from tcode_engine.proto"
# )

# set(PROTO_SRCS 
#     "${PROTO_GEN_DIR}/tcode_engine.pb.cc" 
#     "${PROTO_GEN_DIR}/tcode_engine.grpc.pb.cc"
# )

# -----------------------------------------------------------------------------
# TCodeIME (TSF DLL)
# -----------------------------------------------------------------------------
set(IME_SRC_FILES
    src/dllmain.cpp
    src/Globals.cpp
    src/Registry.cpp
    src/ClassFactory.cpp
    src/TCodeIME.cpp
    src/IPCClient.cpp  # New Named Pipe Client
    src/Resources.rc
    src/TCodeIME.def
)

add_library(TCodeIME SHARED ${IME_SRC_FILES})

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
set_target_properties(TCodeIME PROPERTIES PREFIX "")

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
    find_program(POWERSHELL_EXECUTABLE powershell)
    if(NOT POWERSHELL_EXECUTABLE)
        message(WARNING "powershell.exe not found. The 'TCodeMSIX' target will not be available.")
    else()
        add_custom_target(TCodeMSIX
            COMMAND ${POWERSHELL_EXECUTABLE}
                -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/packaging/build_package.ps1"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/packaging"
            DEPENDS TCodeIME TCodeProxy
            COMMENT "Building signed MSIX package using winapp CLI ($<CONFIG>)"
        )
    endif()
endif()

