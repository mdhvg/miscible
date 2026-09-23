-- dynamic
target("dynamic")
set_kind("binary")
add_rules("mode.release")
set_optimize("fastest")
set_basename("bin2c")
add_files("../scripts/bin2c.c")
after_build(function()
	os.mkdir("src/_dynamic")
	os.execv("build/bin2c", { "data/icon.png", "icon", "src/_dynamic/icon.c" })
	os.execv("build/bin2c", { "fonts/lucide.ttf", "lucide_font", "src/_dynamic/lucide.c" })
	os.execv("build/bin2c", { "fonts/Geist.ttf", "geist_font", "src/_dynamic/geist.c" })
	os.execv("build/bin2c", { "fonts/InstrumentSans.ttf", "instrumentsans_font", "src/_dynamic/instrument.c" })

	import("lib.detect.find_tool")
	local generated = 0

	local uv = find_tool("uv")
	if uv then
		os.execv("uv", { "run", "scripts/gen_manifest.py", "manifest.json", "src/_dynamic/manifest.cpp" })
		generated = 1
	end

	local python = find_tool("python")
	if python then
		os.execv("python", { "scripts/gen_manifest.py", "manifest.json", "src/_dynamic/manifest.cpp" })
		generated = 1
	end

	local python3 = find_tool("python3")
	if python3 then
		os.execv("python3", { "scripts/gen_manifest.py", "manifest.json", "src/_dynamic/manifest.cpp" })
		generated = 1
	end

	if generated == 0 then
		raise("Error: Neither 'uv' nor 'python' was found to run gen_manifest.py")
	end
end)
