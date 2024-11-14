find_package(LLVM CONFIG)
find_package(embree 3 CONFIG)

if (LLVM_FOUND AND embree_FOUND)
    set(LUISA_COMPUTE_ENABLE_FALLBACK ON)
    set(LC_BACKEND_FALLBACK_SRC
            fallback_device.cpp
            #llvm_codegen.cpp
            #llvm_codegen_value.cpp
            #llvm_codegen_builtin.cpp
            #llvm_codegen_type.cpp
            #llvm_codegen_expr.cpp
            #llvm_codegen_func.cpp
            #llvm_codegen_stmt.cpp
            #llvm_event.cpp
            #llvm_shader.cpp
            thread_pool.cpp
            fallback_bindless_array.cpp
            fallback_stream.cpp
            fallback_texture.cpp
            fallback_mesh.cpp
            fallback_accel.cpp
            fallback_codegen.cpp
            )
    luisa_compute_add_backend(fallback SOURCES ${LC_BACKEND_FALLBACK_SRC})
    message(STATUS "Build with fallback backend (LLVM ${LLVM_VERSION}, Embree ${embree_VERSION})")
    llvm_map_components_to_libnames(LLVM_LIBS core executionengine support orcjit nativecodegen irreader passes analysis coroutines)
    target_include_directories(luisa-compute-backend-fallback PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(luisa-compute-backend-fallback PRIVATE embree ${LLVM_LIBS} luisa-compute-xir)
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(luisa-compute-backend-fallback PRIVATE
                /wd4624 /wd4996 # do not complain about LLVM
                /fp:fast /fp:contract /fp:except-)
    else ()
        target_compile_options(luisa-compute-backend-fallback PRIVATE -ffast-math)
    endif ()
elseif (NOT LUISA_COMPUTE_CHECK_BACKEND_DEPENDENCIES)
    message(FATAL_ERROR "LLVM or Embree not found for the fallback backend.")
else ()
    message(WARNING "LLVM or Embree not found. Fallback backend will not be built.")
endif ()
