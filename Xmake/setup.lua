task("deps")
on_run(function()
	function download_and_extract(module)
		import("net.http")
		import("utils.archive")

		local dl_path = path.join(module.parent, module.filename)
		if not os.exists(dl_path) then
			if not os.exists(module.parent) then
				os.mkdir(module.parent)
			end
			cprintf("${bright}Downloading %s...${clear}\n", module.url)
			http.download(module.url, dl_path)
		end

		if module.hash then
			local current_hash = hash.sha256(dl_path)
			if current_hash ~= module.hash then
				os.rm(dl_path)
				raise(
					string.format(
						"Hash mismatch for %s!\nExpected: %s\nGot: %s",
						module.filename,
						module.hash,
						current_hash
					)
				)
			end
		end

		if module.extract then
			local tmp_dir = path.join(module.parent, "tmp")
			if module.flat then
				cprintf("${green}Extracting %s (Flat) to %s...${clear}\n", dl_path, module.parent)
				os.mkdir(tmp_dir)
				archive.extract(dl_path, tmp_dir)
				for _, file in ipairs(os.files(path.join(tmp_dir, "**"))) do
					local name = path.filename(file)
					os.mv(file, path.join(module.parent, name))
				end
			else
				cprintf("${green}Extracting %s (Structured) to %s...${clear}\n", dl_path, module.parent)
				archive.extract(dl_path, tmp_dir)

				local subdirs = os.dirs(path.join(tmp_dir, "*"))
				if #subdirs > 0 then
					local inner_dir = subdirs[1]
					for _, item in ipairs(os.filedirs(path.join(inner_dir, "*"))) do
						os.mv(item, module.parent)
					end
				end
			end
			os.rm(tmp_dir)
		end
	end

	local modules = {
		{
			url = "https://sqlite.org/2025/sqlite-amalgamation-3490100.zip",
			filename = "sqlite.zip",
			parent = "deps/sqlite",
			hash = "6cebd1d8403fc58c30e93939b246f3e6e58d0765a5cd50546f16c00fd805d2c3",
			extract = 1,
			flat = 1,
		},
		{
			url = "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h",
			filename = "stb_image.h",
			parent = "deps/stb",
		},
		{
			url = "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h",
			filename = "stb_image_write.h",
			parent = "deps/stb",
		},
		{
			url = "https://raw.githubusercontent.com/nothings/stb/master/stb_image_resize2.h",
			filename = "stb_image_resize2.h",
			parent = "deps/stb",
		},
		{
			url = "https://raw.githubusercontent.com/juliettef/IconFontCppHeaders/refs/heads/main/IconsLucide.h",
			filename = "IconsLucide.h",
			parent = "deps/icons",
		},
		{
			url = "https://unpkg.com/lucide-static@latest/font/lucide.ttf",
			filename = "lucide.ttf",
			parent = "fonts",
		},
		{
			url = "https://raw.githubusercontent.com/doctest/doctest/1da23a3e8119ec5cce4f9388e91b065e20bf06f5/doctest/doctest.h",
			filename = "doctest.h",
			parent = "deps/doctest",
		},
		{
			url = "https://raw.githubusercontent.com/ogay/sha2/b90991f90967a46d0955dc981e9e3cd53c13b061/sha2.c",
			filename = "sha2.c",
			parent = "deps/sha2",
		},
		{
			url = "https://raw.githubusercontent.com/ogay/sha2/b90991f90967a46d0955dc981e9e3cd53c13b061/sha2.h",
			filename = "sha2.h",
			parent = "deps/sha2",
		},
		{
			url = "https://raw.githubusercontent.com/zserge/jsmn/refs/heads/master/jsmn.h",
			filename = "jsmn.h",
			parent = "deps/jsmn",
		},
	}
	for _, item in ipairs(modules) do
		download_and_extract(item)
	end

	if is_plat("windows") then
		ovino_url =
			"https://storage.openvinotoolkit.org/repositories/openvino/packages/2026.2.1/windows/openvino_toolkit_windows_2026.2.1.21919.ede283a88e3_x86_64.zip"
		ovino_filename = "openvino.zip"
		ovino_hash = "7de2d4992979e872077be559b72f1ca38f1bd3c4d1a1114b775ee43d61d2139c"
	elseif is_plat("linux") then
		ovino_url =
			"https://storage.openvinotoolkit.org/repositories/openvino/packages/2026.2.1/linux/openvino_toolkit_rhel8_2026.2.1.21919.ede283a88e3_x86_64.tgz"
		ovino_filename = "openvino.tgz"
		ovino_hash = "cf7a3eb84a1edbd852f719a4ba8c15dbf02a3744ab61488b6cae44747d90bf78"
	end

	local openvino = {
		url = ovino_url,
		filename = ovino_filename,
		parent = "deps/openvino",
		hash = ovino_hash,
		extract = 1,
		flat = false,
	}
	download_and_extract(openvino)

	cprintf("${bright green}All dependencies setup successfully!${clear}\n")
end)

target("setup")
set_kind("phony")
on_build(function(target)
	import("core.project.task")
	task.run("deps")
end)
