includes("Xmake/setup.lua")
includes("Xmake/cmake-build.lua")
includes("Xmake/config-setup.lua")

add_rules("mode.debug", "mode.release")

set_languages("c++20")
if is_plat("windows") then
	add_cxflags("/Zc:preprocessor", { force = true })
end

set_targetdir("$(projectdir)/build")
set_objectdir("$(projectdir)/build")

add_defines("SQLITE_CORE=1")
add_defines("CURL_STATICLIB")
add_defines('IMGUI_USER_CONFIG="app/mscbl_imconfig.h"')
add_defines("_CRT_SECURE_NO_WARNINGS=1")

if not is_mode("release") then
	add_defines("DBG=1")
	add_defines('ROOT_DIR="' .. os.projectdir():gsub("\\", "/") .. '"')
end

add_includedirs("src")
add_includedirs("deps/stb")
add_includedirs("deps/sha2")
add_includedirs("deps/icons")
add_includedirs("deps/imgui")
add_includedirs("deps/sqlite")
add_includedirs("deps/glad/src")
add_includedirs("deps/glad/include")
add_includedirs("deps/glfw/include")
add_includedirs("deps/ggml/include")
add_includedirs("deps/curl/include")
add_includedirs("deps/tinyfiledialogs")
add_includedirs("deps/libfyaml/include")
add_includedirs("deps/usearch/sqlite")
add_includedirs("deps/usearch/include")
add_includedirs("deps/usearch/fp16/include")
add_includedirs("deps/usearch/stringzilla/include")
add_includedirs("deps/onnxruntime/include")
add_includedirs("deps/onnxruntime/include/onnxruntime/core/session")
add_includedirs("deps/onnxruntime-extensions/include")

-- tests
target("tests")
set_kind("binary")
add_defines("MSCBL_CORE=1")
add_files("tests/all.cpp")
add_files("tests/**/*.cpp")
add_includedirs("deps/doctest")

-- C target
target("ctarget")
set_kind("binary")
set_languages("c++20", "c11")
add_defines("MSCBL_CORE=1")
add_files("src/ctarget.c")
add_files("src/os/os_inc.cpp")
add_files("src/base/arena.cpp")
add_files("src/base/string.cpp")
-- add_files("src/base/ringbuf.cpp")

-- Miscible.i
target("preprocess")
set_kind("phony")
on_build(function(target)
	import("core.tool.compiler")
	import("core.project.depend")
	local sourcefile = "src/app/miscible.cpp"
	local outputfile = "build/miscible.i"
	depend.on_changed(function()
		local flags = compiler.compflags(sourcefile, { target = target })
		table.insert(flags, "-E")
		os.runv(target:tool("cxx"), table.join(flags, sourcefile), { stdout = outputfile })
	end, { files = sourcefile })
end)

target("libmiscible_c")
set_kind("static")
add_defines("MSCBL_CORE=1")
add_files("src/deps_unity.c")
add_files("src/app/miscible.c")

-- libmiscible.dll
target("libmiscible")
set_kind("shared", { inherited = true })
add_defines("MSCBL_CORE=1")
add_files("src/deps_unity.cpp")
add_files("src/app/miscible.cpp")

add_links("noexcep_operators.lib")
add_links("ocos_operators.lib")
add_links("ortcustomops.lib")
add_links("ortextensions.lib")
add_links("build/onnx*")

add_deps("preprocess")
add_deps("libmiscible_c")

add_packages("glfw_local")
add_packages("ggml_local")
add_packages("curl_local")
add_packages("libfyaml_local")

-- pages_###.dll
target("pages")
set_kind("shared")
set_prefixname("")
if is_plat("windows") then
	math.randomseed(os.time())
	set_basename("pages_" .. math.random(0, 9999))
end
before_build(function(target)
	if is_plat("windows") then
		local pattern = path.join(target:targetdir(), "*pages_*")
		cprintf("${bright red}Deleting all files matching %s${clear}\n", pattern)
		for _, file in ipairs(os.files(pattern)) do
			try({
				function()
					os.rm(file)
				end,
			})
		end
	end
end)
add_deps("libmiscible")
add_files("src/ui/pages/pages.cpp")

-- Miscible.exe
target("miscible")
set_kind("binary")
set_basename("Miscible")
add_rules("create_config")
add_files("src/main.cpp")
if is_mode("release") then
	if is_plat("windows") then
		add_files("build/resources.rc")
		on_config(function(target)
			import("core.project.project")
			local rc_dir = "build"
			if not os.isdir(rc_dir) then
				os.mkdir(rc_dir)
			end
			local ico_path = path.absolute("data/Miscible.ico"):gsub("/", "\\\\")
			local rc_content = string.format('1 ICON "%s"\n', ico_path)
			io.writefile("build/resources.rc", rc_content, { encoding = "ascii" })
		end)
	end

	add_files("src/app/miscible.c")
	add_files("src/app/miscible.cpp")
	add_files("src/deps_unity.c")
	add_files("src/deps_unity.cpp")
	add_files("src/ui/pages/pages.cpp")

	add_packages("glfw_local")
	add_packages("ggml_local")
	add_packages("curl_local")
	add_packages("libfyaml_local")

	add_links("noexcep_operators.lib")
	add_links("ocos_operators.lib")
	add_links("ortcustomops.lib")
	add_links("ortextensions.lib")
	add_links("build/onnx*")

	add_deps("onnxruntime", { inherited = false })
else
	add_deps("pages", { inherited = false })
	add_deps("libmiscible")
end
