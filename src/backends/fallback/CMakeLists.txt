find_package(LLVM CONFIG)
find_package(embree CONFIG)

if(NOT embree_FOUND)
    message(WARNING "Embree not found. Fallback backend will not be built.")
endif()

if (NOT LLVM_FOUND)
    message(WARNING "LLVM not found. Fallback backend will not be built.")
endif()

if (LLVM_FOUND AND embree_FOUND)
    set(LUISA_COMPUTE_ENABLE_FALLBACK ON)
    set(LC_BACKEND_FALLBACK_SRC
            fallback_device.cpp
            fallback_device_api.cpp
            fallback_device_api_ir_module.cpp
            fallback_command_queue.cpp
            #llvm_codegen.cpp
            #llvm_codegen_value.cpp
            #llvm_codegen_builtin.cpp
            #llvm_codegen_type.cpp
            #llvm_codegen_expr.cpp
            #llvm_codegen_func.cpp
            #llvm_codegen_stmt.cpp
            #llvm_event.cpp
            #llvm_shader.cpp
            #dirty_range.cpp
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

    #exclude codegen from unity build since it's too big
    set_source_files_properties(fallback_codegen.cpp PROPERTIES UNITY_BUILD OFF)

    luisa_compute_add_backend(fallback SOURCES ${LC_BACKEND_FALLBACK_SRC})

    # maybe we can use tbb?
    find_package(TBB CONFIG)
    if (TBB_FOUND)
        message(STATUS "Use TBB for the fallback backend.")
        target_link_libraries(luisa-compute-backend-fallback PRIVATE TBB::tbb)
        target_compile_definitions(luisa-compute-backend-fallback PRIVATE LUISA_COMPUTE_ENABLE_TBB)
    endif ()

    #set_target_properties(luisa-compute-backend-fallback PROPERTIES UNITY_BUILD ON)
    message(STATUS "Build with fallback backend (LLVM ${LLVM_VERSION}, Embree ${embree_VERSION})")
    llvm_map_components_to_libnames(LLVM_LIBS core executionengine support orcjit nativecodegen irreader passes analysis coroutines)
    target_include_directories(luisa-compute-backend-fallback PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(luisa-compute-backend-fallback PRIVATE
            embree ${LLVM_LIBS}
            luisa-compute-xir
            luisa-compute-vulkan-swapchain)
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(luisa-compute-backend-fallback PRIVATE
                /wd4624 /wd4996 # do not complain about LLVM
                /fp:fast /fp:contract /fp:except-)
    else ()
        target_compile_options(luisa-compute-backend-fallback PRIVATE -ffast-math)
    endif ()
    # help distinguish between embree 3 and 4
    target_compile_definitions(luisa-compute-backend-fallback PRIVATE
            LUISA_COMPUTE_EMBREE_VERSION=${embree_VERSION_MAJOR})
elseif (NOT LUISA_COMPUTE_CHECK_BACKEND_DEPENDENCIES)
    message(FATAL_ERROR "LLVM or Embree not found for the fallback backend.")
else ()
    message(WARNING "LLVM or Embree not found. Fallback backend will not be built.")
endif ()
