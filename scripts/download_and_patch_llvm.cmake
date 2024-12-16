set(LLVM_WIN_BINARY_URL "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.5/clang+llvm-19.1.5-x86_64-pc-windows-msvc.tar.xz")
message(STATUS "LLVM not found. Downloading official prebuilt binaries from ${LLVM_WIN_BINARY_URL}.")
include(FetchContent)
FetchContent_Declare(
    llvm
    URL ${LLVM_WIN_BINARY_URL}
)
FetchContent_MakeAvailable(llvm)

# Wait, we need to patch something

# First locate diaguids.lib
# which is <path to vs 2022>\DIA SDK\lib\amd64\diaguids.lib

# Get the directory of the current C++ compiler
get_filename_component(CXX_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

# Find the Visual Studio installation root
# The CXX_COMPILER path will look something like:
# C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.x.x/bin/Hostx64/x64/cl.exe
string(REGEX REPLACE "/VC/.*" "" VS_INSTALL_DIR "${CXX_COMPILER_DIR}")

# Construct the path to the DIA SDK lib directory
set(DIA_SDK_LIB_DIR "${VS_INSTALL_DIR}/DIA SDK/lib/amd64")

# Check if diaguids.lib exists in the constructed path
find_file(DIAGUIDS_LIB_FOUND "diaguids.lib" PATHS "${DIA_SDK_LIB_DIR}" NO_DEFAULT_PATH)

if(DIAGUIDS_LIB_FOUND)
    message(STATUS "Found diaguids.lib at: ${DIAGUIDS_LIB_FOUND}")
    set(LLVM_DIAGUIDS_LIB "${DIAGUIDS_LIB_FOUND}" CACHE STRING "Path to diaguids.lib")
else()
    message(FATAL_ERROR "diaguids.lib not found in expected path: ${DIA_SDK_LIB_DIR}; Patching failed, please build LLVM from source on your own.")
endif()

set(LLVM_EXPORT_CMAKE ${llvm_SOURCE_DIR}/lib/cmake/llvm/LLVMExports.cmake)

file(READ "${LLVM_EXPORT_CMAKE}" FILE_CONTENTS)

# Define a regex to find the INTERFACE_LINK_LIBRARIES line
set(TARGET_REGEX "INTERFACE_LINK_LIBRARIES \".*diaguids\\.lib;")

# Replace the old path with the new path
string(REGEX REPLACE
"${TARGET_REGEX}" 
"INTERFACE_LINK_LIBRARIES \"${LLVM_DIAGUIDS_LIB};"
NEW_CONTENTS 
"${FILE_CONTENTS}")

# Write the updated contents back to the file
file(WRITE "${LLVM_EXPORT_CMAKE}" "${NEW_CONTENTS}")
message(STATUS "Using patched LLVM at ${llvm_SOURCE_DIR}")
set(LLVM_DIR ${llvm_SOURCE_DIR}/lib/cmake/llvm)
find_package(LLVM REQUIRED CONFIG PATHS ${LLVM_DIR})

