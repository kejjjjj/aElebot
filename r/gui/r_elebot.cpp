#include "main.hpp"
#include "r_Elebot.hpp"
#include "net/nvar_table.hpp"
#include <r/gui/r_main_gui.hpp>
#include <iostream>
#include <shared/sv_shared.hpp>


CElebotWindow::CElebotWindow(const std::string& name)
	: CGuiElement(name) {

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

	const auto table = NVarTables::Get(NVAR_TABLE_NAME);

	if (!table)
		return;

	for (const auto& v : *table) {

		const auto& [key, value] = v;

		if (value && value->IsImNVar()) {
			std::string _v_ = key + "_menu";
			ImGui::BeginChild(_v_.c_str(), ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Border);
			value->RenderImNVar();
			ImGui::EndChild();
		}

	}
}

