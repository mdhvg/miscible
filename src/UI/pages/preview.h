#include "imgui.h"

#include "base/base_core.h"
#include "ui/ui_core.h"

// void ui_preview()
// {
// 	if (ImGui::IsKeyPressed(ImGuiKey_Escape))
// 	{
// 		state.view = MENU;
// 		// restore_scroll = true;
// 	}

// 	if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
// 	{
// 		if (active_index > 0)
// 		{
// 			state.active_image = order[--active_index];
// 		}
// 	}

// 	if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
// 	{
// 		if (active_index < order.size() - 1)
// 		{
// 			state.active_image = order[++active_index];
// 		}
// 	}

// 	ImGui_ImplOpenGL3_NewFrame();
// 	ImGui_ImplGlfw_NewFrame();
// 	ImGui::NewFrame();

// 	ImGuiViewport *main_viewport = ImGui::GetMainViewport();
// 	ImGui::SetNextWindowPos(main_viewport->Pos);
// 	ImGui::SetNextWindowSize(main_viewport->Size);
// 	ImGui::SetNextWindowViewport(main_viewport->ID);
// 	ImGui::Begin("Main Window", nullptr,
// 				 ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
// 					 ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar);

// 	ImGui::BeginMenuBar();
// 	if (ImGui::BeginMenu("File"))
// 	{
// 		if (ImGui::MenuItem("Exit"))
// 			glfwSetWindowShouldClose(win.handle, GLFW_TRUE);
// 		ImGui::EndMenu();
// 	}
// 	ImGui::EndMenuBar();

// 	ImVec2 avail = ImGui::GetContentRegionAvail();
// 	// ImageTexture& tex = ImageManager::preview_texture;

// 	// float iw = (float)tex.width;
// 	// float ih = (float)tex.height;

// 	// float scale = std::min(avail.x / iw, avail.y / ih);

// 	// ImVec2 size(iw * scale, ih * scale);

// 	// float offset_x = (avail.x - size.x) * 0.5f;
// 	// if (offset_x > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);

// 	// ImGui::Image((ImTextureID)(intptr_t)tex.texture_id, size, ImVec2(0, 0),
// 	//              ImVec2(1, 1));

// 	ImGui::End();

// 	// Rendering
// 	ImGui::Render();
// 	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

// 	// 	// Update and Render additional Platform Windows
// 	// 	// Platform functions may change the current OpenGL context, so we
// 	// 	// save/restore it to make it easier to paste this code elsewhere.
// 	// 	ImGuiIO &io = ImGui::GetIO();
// 	// 	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
// 	// 		GLFWwindow *backup_current_context = glfwGetCurrentContext();
// ImGui::UpdatePlatformWindows();
// 	// 		ImGui::RenderPlatformWindowsDefault();
// 	// 		glfwMakeContextCurrent(backup_current_context);
// 	// 	}
// }
