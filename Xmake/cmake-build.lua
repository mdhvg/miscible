package("glfw_local")
set_sourcedir("deps/glfw")
on_install(function(package)
	local configs = {
		"-GNinja",
		"-DGLFW_BUILD_EXAMPLES=OFF",
		"-DGLFW_BUILD_TESTS=OFF",
		"-DGLFW_BUILD_DOCS=OFF",
		"-DBUILD_SHARED_LIBS=OFF",
	}
	import("package.tools.cmake").install(package, configs)
end)
package_end()
add_requires("glfw_local")

package("ggml_local")
set_sourcedir("deps/ggml")
on_install(function(package)
	local configs = {
		"-GNinja",
		"-DGGML_BUILD_SHARED_LIBS=OFF",
		"-DGGML_OPENMP=OFF",
	}
	import("package.tools.cmake").install(package, configs)
end)
package_end()
add_requires("ggml_local")

package("curl_local")
set_sourcedir("deps/curl")
add_links("libcurl")
on_install(function(package)
	local configs = {
		"-GNinja",
		"-DBUILD_CURL_EXE=OFF",
		"-DBUILD_TESTING=OFF",
		"-DBUILD_STATIC_LIBS=ON",
		"-DBUILD_SHARED_LIBS=OFF",
		"-DCURL_DISABLE_INSTALL=OFF",
		"-DCURL_ENABLE_SSL=ON",
		"-DCURL_USE_LIBPSL=OFF",
		"-DCURL_WERROR=OFF",
		"-DPICKY_COMPILER=OFF",
		"-DBUILD_EXAMPLES=OFF",
		"-DCURL_USE_SCHANNEL=ON",
	}
	import("package.tools.cmake").install(package, configs)
end)
package_end()
add_requires("curl_local")

package("libfyaml_local")
set_sourcedir("deps/libfyaml")
on_install(function(package)
	import("package.tools.cmake").install(package, configs)
end)
package_end()
add_requires("libfyaml_local")

target("onnxruntime")
set_kind("phony")
on_build(function(target)
	if is_plat("windows") then
		import("core.base.option")

		local cfg = "Debug"
		if is_mode("release") then
			cfg = "Release"
		end

		local args = {
			"run",
			"deps/onnxruntime/tools/ci_build/build.py",
			"--config",
			cfg,
			"--update",
			"--build",
			"--build_shared_lib",
			"--parallel",
			"--compile_no_warning_as_error",
			"--skip_submodule_sync",
			"--skip_tests",
			"--use_dml",
			"--use_openvino",
			"--cmake_generator",
			"Ninja",
			"--build_dir",
			"build/onnxruntime",

			"--targets",
			"onnxruntime",
			"onnxruntime_providers",
			"onnxruntime_providers_dml",
			"onnxruntime_providers_openvino",
			"onnxruntime_providers_shared",

			"--cmake_extra_defines",
			"onnxruntime_ENABLE_TRAINING=OFF",
			"onnxruntime_ENABLE_TRAINING_APIS=OFF",
			"onnxruntime_ENABLE_TRAINING_OPS=OFF",
			"onnxruntime_BUILD_BENCHMARKS=OFF",
			"onnxruntime_USE_UNITY_BUILD=OFF",
			"ONNX_USE_UNITY_BUILD=OFF",
			"OpenVINO_DIR=" .. path.join(os.projectdir(), "deps/openvino/runtime/cmake"),
		}

		cprintf("${bright green}Building ONNXRuntime via Python script...${clear}\n")
		print("uv", table.unpack(args))
		os.execv("uv", args)

		cprintf("${bright green}Building ONNXRuntime-Extensions manually...${clear}\n")
		os.execv("cmake", {
			"-S",
			"deps/onnxruntime-extensions",
			"-B",
			"build/onnxruntime-extensions",
			"-GNinja",
			"-DCMAKE_BUILD_TYPE=RelWithDebInfo",

			"-DOCOS_ENABLE_C_API=ON",
			"-DOCOS_BUILD_SHARED_LIB=ON",
			"-DOCOS_ENABLE_GPT2_TOKENIZER=ON",
			"-DOCOS_ENABLE_SPM_TOKENIZER=OFF",
			"-DOCOS_ENABLE_AUDIO=ON",
			"-DOCOS_ENABLE_VISION=ON",
			"-DOCOS_ENABLE_DLIB=ON",
		})

		os.execv("cmake", {
			"--build",
			"build/onnxruntime-extensions",
		})

		local app_bin_dir = target:targetdir()

		cprintf("${bright green}Copying required ONNXRuntime DLLs to %s...${clear}\n", app_bin_dir)
		os.cp(path.join("build/onnxruntime", cfg, "*.dll"), app_bin_dir)
		os.cp(path.join("build/onnxruntime", cfg, "*.lib"), app_bin_dir)

		os.cp(path.join("build/onnxruntime-extensions/lib", "*.dll"), app_bin_dir)
		os.cp(path.join("build/onnxruntime-extensions/lib", "*.lib"), app_bin_dir)
	end
end)
