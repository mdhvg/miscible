rule("create_config")
before_build(function(target)
	local project_dir = os.projectdir()

	local debug_config = path.join(project_dir, "config.debug.yaml")
	local release_config = path.join(project_dir, "config.release.yaml")
	local target_config = path.join(project_dir, "src/config.yaml")

	if os.exists(debug_config) then
		print("Found config.debug.yaml, copying to src/config.yaml...")
		os.cp(debug_config, target_config)
	elseif os.exists(release_config) then
		print("Found config.release.yaml, copying to src/config.yaml...")
		os.cp(release_config, target_config)
	else
		raise("Build Error: Neither config.debug.yaml nor config.release.yaml was found in the project root!")
	end
end)
