#include "IconsLucide.h"
#include "base/array.h"
#include "imgui.h"

#include "base/base_core.h"
#include "ui/ui_core.h"
#include "UI/components/Button.h"
#include "ui/components/Sidebar.h"

internal void ui_menu()
{
	ImGuiIO &io = ImGui::GetIO();
	ImGui::SetNextWindowPos({0, 0});
	ImGui::SetNextWindowSize(io.DisplaySize);

	ImGui::PushFont(ui_persist.ui_font);
	ImGui::PushFont(ui_persist.icon_font);

	ImGui::Begin("Window", NULL,
				 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
					 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
					 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_MenuBar);

	ImGui::BeginMenuBar();
	if (ImGui::BeginMenu("File"))
	{
		if (ImGui::MenuItem("Exit"))
			glfwSetWindowShouldClose(win.handle, GLFW_TRUE);
		ImGui::EndMenu();
	}
	ImGui::EndMenuBar();

	SIDEBAR({
		ImGui::BeginChild("Sidebar", {ui_persist.sidebar_width, ImGui::GetContentRegionAvail().y});
		ImGui::SetCursorPos({4, 4});
		BUTTON_GHOST({
			if (ImGui::Button(ui_persist.sidebar_open ? ICON_LC_CHEVRON_LEFT : ICON_LC_CHEVRON_RIGHT, {40, 0}))
			{
				ui_persist.sidebar_open = !ui_persist.sidebar_open;
			}
		})
		ImGui::EndChild();
	});

	ImGui::SameLine();
	{
		ImGui::BeginChild("Grid");

		// ImGui::PushFont(ui_persist.title_font);
		// ImGui::Text(APP_NAME);
		// ImGui::PopFont();

		// ImGui::Text("FPS: %.2f", state.fps);

		// if (ImGui::BeginCombo("##sort_order", "Sort ", ImGuiComboFlags_WidthFitPreview))
		// {
		// 	for (int n = 0; n < SORT_COUNT; n++)
		// 	{
		// 		SortMode val = (SortMode)n;
		// 		bool is_selected = (state.sorting == val);
		// 		if (ImGui::Selectable(SortToString(val), is_selected))
		// 			state.sorting = val;
		// 		if (is_selected)
		// 			ImGui::SetItemDefaultFocus();
		// 	}
		// 	ImGui::EndCombo();
		// }

		for (U32 i = 0; i < ui_persist.texture_data.size; i++)
		{
			ImGui::Image(dyn_array_at(ui_persist.texture_data, i).texture_id, ImVec2(ATLAS_SIZE / 4, ATLAS_SIZE / 4));
		}

		float avail	  = ImGui::GetContentRegionAvail().x;
		float item_w  = 112.0f;
		float spacing = 2 * (ImGui::GetStyle().ItemSpacing.x);

		int cells = (int)((avail + spacing) / (item_w + spacing));
		cells	  = MAX(cells, 1);

		float used = cells * item_w + (cells - 1) * spacing;

		float offset_x = (avail - used) * 0.5f;
		if (offset_x < 0.0f)
			offset_x = 0.0f;

		// Apply the offset
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

		// int inRow = cells;

		// for (int i = 0; i < order.size(); i++) {
		// 	// TODO: Make 224 a variable
		// 	unsigned int id = order[i];
		// 	const auto img = img_man.images[id];
		// 	int x = img.image_index % 10;
		// 	int y = img.image_index / 10;
		// 	if (ImGui::ImageButton(img.path.c_str(),
		// 					img_man.atlas_texture[img.texture_id],
		// 					{ 112, 112 },
		// 					{ (float)x / 10, (float)y / 10 },
		// 					{ (float)(x + 1) / 10, (float)(y + 1) / 10 })) {
		// 		state.active_image = id;
		// 		active_index = i;
		// 		scroll_y = ImGui::GetScrollY();
		// 		state.view = PREVIEW;
		// 	}
		// 	if (--inRow) {
		// 		ImGui::SameLine();
		// 	} else {
		// 		ImGui::SetCursorPosX((avail - used) * 0.5);
		// 		inRow = cells;
		// 	}
		// }

		ImGui::EndChild();
	}
	ImGui::End();

	ImGui::PopFont();
	ImGui::PopFont();

	// if (restore_scroll) {
	// 	ImGui::SetScrollY(scroll_y);
	// 	restore_scroll = false;
	// }

	// ImGui::BeginMenuBar();
	// if (ImGui::BeginMenu("File")) {
	// 	if (ImGui::MenuItem("Exit"))
	// 		glfwSetWindowShouldClose(glfwWin, GLFW_TRUE);
	// 	ImGui::EndMenu();
	// }
	// ImGui::EndMenuBar();

	// ImGui::Begin("Thumbnails");

	// float curTime = glfwGetTime();
	// // ImGui::InputTextWithHint("Search", "describe image...", searchField,
	// 2048); ImGui::End(); ImGui::End();

	// Rendering
	// ImGui::Render();
	// ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	// 	// Update and Render additional Platform Windows
	// 	// Platform functions may change the current OpenGL context, so we
	// 	// save/restore it to make it easier to paste this code elsewhere.
	// 	ImGuiIO &io = ImGui::GetIO();
	// if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	// 	GLFWwindow *backup_current_context = glfwGetCurrentContext();
	// ImGui::UpdatePlatformWindows();
	// ImGui::RenderPlatformWindowsDefault();
	// 	glfwMakeContextCurrent(backup_current_context);
	// }
}
