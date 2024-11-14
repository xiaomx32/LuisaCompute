find_package(LLVM CONFIG)
find_package(embree REQUIRED)

if (LLVM_FOUND) # link with llvm for jit
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
            )
    luisa_compute_add_backend(fallback SOURCES ${LC_BACKEND_FALLBACK_SRC})
    message(STATUS "Build with fallback backend")
    llvm_map_components_to_libnames(LLVM_LIBS core executionengine support orcjit nativecodegen irreader passes analysis coroutines)
    target_include_directories(luisa-compute-backend-fallback PRIVATE ${LLVM_INCLUDE_DIRS})
    target_link_libraries(luisa-compute-backend-fallback PRIVATE ${LLVM_LIBS})
    target_link_libraries(luisa-compute-backend-fallback PRIVATE embree)
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(luisa-compute-backend-fallback PRIVATE
                /wd4624 /wd4996 # do not complain about LLVM
                /fp:fast /fp:contract /fp:except-)
    else ()
        target_compile_options(luisa-compute-backend-fallback PRIVATE -ffast-math)
    endif ()
elseif (NOT LUISA_COMPUTE_CHECK_BACKEND_DEPENDENCIES)
    message(FATAL_ERROR "LLVM not found for the fallback backend.")
else ()
    message(WARNING "LLVM not found. Fallback backend will not be built.")
endif ()