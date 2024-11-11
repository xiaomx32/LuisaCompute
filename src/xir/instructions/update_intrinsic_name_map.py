import os
import functools

if __name__ == "__main__":
    # get current script directory
    script_dir: str = os.path.dirname(os.path.realpath(__file__))
    src_index = script_dir.rfind("src")
    assert src_index != -1, "src not found in the path"
    header_path = os.path.join(script_dir[:src_index], "include", "luisa", "xir", "instructions", "intrinsic.h")
    with open(header_path, "r") as f:
        lines = [x for line in f.readlines() if (x := line.strip()) and not x.startswith("//")]
        enum_index = lines.index("enum struct IntrinsicOp {") + 1
        lines = lines[enum_index:]
        enum_end = lines.index("};")
        ops = [line.split(",")[0].strip() for line in lines[:enum_end]]
    with open(os.path.join(script_dir, "intrinsic_name_map.inl.h"), "w") as file:
        file.write("""#pragma once

luisa::string to_string(IntrinsicOp op) noexcept {
    switch (op) {
""")
        file.writelines([f"        case IntrinsicOp::{op}: return \"{op.lower()}\";\n" for op in ops])
        file.write("""    }
    LUISA_ERROR_WITH_LOCATION("Unknown intrinsic operation: {}.",
                              static_cast<uint32_t>(op));
}

IntrinsicOp intrinsic_op_from_string(luisa::string_view name) noexcept {
    static const luisa::unordered_map<luisa::string, IntrinsicOp> m{
""")
        file.writelines([f"        {{\"{op.lower()}\", IntrinsicOp::{op}}},\n" for op in ops])
        file.write("""    };
    auto iter = m.find(name);
    LUISA_ASSERT(iter != m.end(), "Unknown intrinsic operation: {}.", name);
    return iter->second;
}
""")
