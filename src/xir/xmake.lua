target("lc-xir")
_config_project({
	project_kind = "shared"
})
add_deps("lc-ast")
add_packages("yyjson", {public = false})
add_files("**.cpp")
add_defines("YYJSON_PACKAGES", "LC_XIR_EXPORT_DLL")