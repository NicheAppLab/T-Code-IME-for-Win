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
# TCodeProxy (Moved to C# / proxy/TCodeProxy)
# -----------------------------------------------------------------------------
# set(PROXY_SRC_FILES
#     src/ProxyMain.cpp
#     src/PipeServer.cpp
#     src/ProcessManager.cpp
#     src/EngineClient.cpp
# )

# add_executable(TCodeProxy ${PROXY_SRC_FILES} ${PROTO_SRCS})

# target_include_directories(TCodeProxy PRIVATE 
#     "${CMAKE_CURRENT_SOURCE_DIR}/src"
#     "${PROTO_GEN_DIR}"
# )

# target_link_libraries(TCodeProxy PRIVATE
#     gRPC::grpc++
#     protobuf::libprotobuf
#     advapi32
# )

# target_compile_definitions(TCodeProxy PRIVATE UNICODE _UNICODE)
