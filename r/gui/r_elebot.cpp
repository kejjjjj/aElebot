#include "main.hpp"
#include "net/im_defaults.hpp"
#include "net/nvar_table.hpp"
#include "r/gui/r_main_gui.hpp"
#include "r_elebot.hpp"
#include "shared/sv_shared.hpp"

CElebotWindow::CElebotWindow(const std::string& name)
	: CGuiElement(name) {

	m_oKeybinds.emplace_back(std::make_unique<ImKeybind>("id_eb_run", "eb_run", "start the lineup (also use this when automatic detection isn't working)"));
	m_oKeybinds.emplace_back(std::make_unique<ImKeybind>("id_eb_centerYaw", "eb_centerYaw", "set yaw to nearest cardinal direction for quick lineups"));
}

void CElebotWindow::Render()
{
#if(!DEBUG_SUPPORT)
	static auto func = CMain::Shared::GetFunctionSafe("GetContext");

	if (!func) {
		func = CMain::Shared::GetFunctionSafe("GetContext");
		return;
	}

	ImGui::SetCurrentContext(func->As<ImGuiContext*>()->Call());

#endif

	if (m_oKeybinds.size()) {
		ImGui::Text("Keybinds");
		for (auto& keybind : m_oKeybinds) {
			keybind->Render();
		}

		ImGui::Separator();
		ImGui::NewLine();
	}

	GUI_RenderNVars();
}

