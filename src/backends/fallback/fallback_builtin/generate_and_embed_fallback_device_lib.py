import os
import subprocess
import platform
import re

if __name__ == "__main__":
    builtin_dir = os.path.dirname(os.path.realpath(__file__))
    system_name = platform.system().lower()
    machine_name = platform.machine().lower()
    file_name = "fallback_device_api_wrappers"
    src_path = os.path.join(builtin_dir, f"{file_name}.cpp")
    dst_path = os.path.join(builtin_dir, f"{file_name}.{system_name}.{machine_name}.ll")
    subprocess.run(["clang++", "-c", "-emit-llvm", "-std=c++20", "-S", src_path, "-o", dst_path, "-fno-stack-protector", "-ffast-math", "-O3"])
    with open(dst_path, "r") as f:
        content = "".join(line for line in f.readlines()
                          if not line.strip().startswith("@llvm.used") and
                          not line.strip().startswith("; ModuleID") and
                          not line.strip().startswith("source_filename") and
                          not line.strip().startswith("target datalayout") and
                          not line.strip().startswith("target triple"))
    content = content.replace("define hidden", "define private")
    # find all @luisa_fallback_wrapper_(\w+) and collect in a list
    impl_prefix = "@luisa_fallback_"
    wrapper_prefix = "@luisa_fallback_wrapper_"
    functions = re.findall(f"{wrapper_prefix}([a-zA-Z0-9_]+)", content)
    for function_name in functions:
        dotted_name = function_name.replace("_", ".")
        print(f"{wrapper_prefix}{function_name} -> @luisa.{dotted_name}")
        content = content.replace(f"{wrapper_prefix}{function_name}(", f"@luisa.{dotted_name}(")
        content = content.replace(f"{impl_prefix}{function_name}(", f"@luisa.{dotted_name}.impl(")
    with open(dst_path, "w") as f:
        f.write(content)
    with open(os.path.join(builtin_dir, "..", "fallback_device_api_map_symbols.inl.h"), "w") as f:
        for function_name in functions:
            dotted_name = function_name.replace("_", ".")
            f.write(f"map_symbol(\"luisa.{dotted_name}.impl\", &api::luisa_fallback_{function_name});\n")
    with open(os.path.join(builtin_dir, f"{file_name}.{system_name}.{machine_name}.inl"), "w") as f:
        data = [f"0x{ord(c):02x}" for c in content.replace("\r\n", "\n")]
        size = len(data)
        f.write(f'\nstatic const char luisa_fallback_backend_device_builtin_module[{size}] = {{\n')
        wrapped = ["    " + ", ".join(data[i: i + 16]) for i in range(0, len(data), 16)]
        f.write(",\n".join(wrapped))
        f.write("\n};\n")
