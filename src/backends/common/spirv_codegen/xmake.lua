target("lc-spirv-codegen")
_config_project({
    project_kind = "static"
})
set_pcxxheader("__pch.h")
add_files("**.cpp")
add_deps("lc-xir")
target_end()