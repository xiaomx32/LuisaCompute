find_package(LLVM CONFIG)
find_package(embree CONFIG)

if (NOT embree_FOUND)
    message(WARNING "Embree not found. Fallback backend will not be built.")
endif ()

if (NOT LLVM_FOUND)
    message(WARNING "LLVM not found. Fallback backend will not be built.")
endif ()

if (LLVM_FOUND AND embree_FOUND)

    message(STATUS "Build with fallback backend (LLVM ${LLVM_VERSION}, Embree ${embree_VERSION})")

    set(LUISA_COMPUTE_ENABLE_FALLBACK ON)
    set(LC_BACKEND_FALLBACK_SRC
            fallback_device.cpp
            fallback_device_api.cpp
            fallback_device_api_ir_module.cpp
            fallback_event.cpp
            fallback_command_queue.cpp
            fallback_bindless_array.cpp
            fallback_stream.cpp
            fallback_texture.cpp
            fallback_mesh.cpp
            fallback_accel.cpp
            fallback_texture_bc.cpp
            fallback_codegen.cpp
            fallback_shader.cpp
            fallback_buffer.cpp
            fallback_swapchain.cpp
    )

    luisa_compute_add_backend(fallback SOURCES ${LC_BACKEND_FALLBACK_SRC})

    # exclude codegen from unity build since it's too big
    set_source_files_properties(fallback_codegen.cpp PROPERTIES UNITY_BUILD OFF)
    target_link_directories(luisa-compute-backend-fallback PRIVATE ${LLVM_LIBRARY_DIRS})
    target_link_libraries(luisa-compute-backend-fallback PRIVATE
            luisa-compute-xir
            luisa-compute-vulkan-swapchain
            embree)

    # help distinguish between embree 3 and 4
    target_compile_definitions(luisa-compute-backend-fallback PRIVATE
            LUISA_COMPUTE_EMBREE_VERSION=${embree_VERSION_MAJOR})

    # use libdispatch or TBB for parallel_for on non-Apple Unix systems if available
    if (UNIX AND NOT APPLE)
        # libdispatch
        find_library(DISPATCH_LIB dispatch)
        find_path(DISPATCH_INCLUDE_DIR dispatch/dispatch.h)
        if (DISPATCH_LIB AND DISPATCH_INCLUDE_DIR)
            target_compile_definitions(luisa-compute-backend-fallback PRIVATE LUISA_COMPUTE_ENABLE_LIBDISPATCH=1)
            target_link_libraries(luisa-compute-backend-fallback PRIVATE ${DISPATCH_LIB})
            target_include_directories(luisa-compute-backend-fallback PRIVATE ${DISPATCH_INCLUDE_DIR})
        else ()
            find_package(TBB CONFIG)
            if (TBB_FOUND)
                target_compile_definitions(luisa-compute-backend-fallback PRIVATE LUISA_COMPUTE_ENABLE_TBB=1)
                target_link_libraries(luisa-compute-backend-fallback PRIVATE TBB::tbb)
            else ()
                message(WARNING "TBB and libdispatch not found. Performance of the fallback backend may be affected.")
            endif ()
        endif ()
    endif ()

    # LLVM
    target_include_directories(luisa-compute-backend-fallback PRIVATE ${LLVM_INCLUDE_DIRS})
    llvm_map_components_to_libnames(LLVM_LIBS core executionengine support orcjit nativecodegen irreader passes analysis coroutines)
    if (UNIX AND NOT APPLE)
        # check if the libraries exist
        # map library dirs to link directories options
        set(LLVM_LINK_DIR_OPTIONS "")
        foreach (dir ${LLVM_LIBRARY_DIRS})
            list(APPEND LLVM_LINK_DIR_OPTIONS "-L${dir}")
        endforeach ()
        try_compile(LLVM_LIBS_EXIST
                SOURCE_FROM_CONTENT "try_link_llvm.cpp" "int main() {}"
                LINK_OPTIONS ${LLVM_LINK_DIR_OPTIONS}
                LINK_LIBRARIES ${LLVM_LIBS})
        if (LLVM_LIB_EXIST)
            message(STATUS "Linking with LLVM libraries: ${LLVM_LIBS}")
        else ()
            message(WARNING "LLVM libraries not found. Trying to link with LLVM shared libraries.")
            try_compile(LLVM_DYLIB_EXISTS
                    SOURCE_FROM_CONTENT "try_link_llvm_dylib.cpp" "int main() {}"
                    LINK_LIBRARIES "LLVM")
            if (LLVM_DYLIB_EXISTS)
                message(STATUS "Linking with LLVM shared library.")
                set(LLVM_LIBS "LLVM")
            else ()
                find_library(LLVM_DYLIB LLVM REQUIRED)
                message(STATUS "LLVM libraries found at ${LLVM_DYLIB}")
                set(LLVM_LIBS ${LLVM_DYLIB})
            endif ()
        endif ()
        # avoid re-exporting symbols from LLVM
        target_link_libraries(luisa-compute-backend-fallback PRIVATE "-Wl,--exclude-libs,ALL" ${LLVM_LIBS})
    else ()
        target_link_libraries(luisa-compute-backend-fallback PRIVATE ${LLVM_LIBS})
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(luisa-compute-backend-fallback PRIVATE
                /wd4624 /wd4996 # do not complain about LLVM
                /fp:fast /fp:contract /fp:except- /ARCH:AVX2)
    else ()
        target_compile_options(luisa-compute-backend-fallback PRIVATE -ffast-math)
    endif ()

elseif (NOT LUISA_COMPUTE_CHECK_BACKEND_DEPENDENCIES)
    message(FATAL_ERROR "LLVM or Embree not found for the fallback backend.")
else ()
    message(WARNING "LLVM or Embree not found. Fallback backend will not be built.")
endif ()
