#pragma once

#include "r/gui/r_gui.hpp"

struct ImKeybind;

class CElebotWindow : public CGuiElement
{
public:
	CElebotWindow(const std::string& id);
	~CElebotWindow() = default;

	void* GetRender() override {
		union {
			void (CElebotWindow::* memberFunction)();
			void* functionPointer;
		} converter{};
		converter.memberFunction = &CElebotWindow::Render;
		return converter.functionPointer;
	}

	void Render() override;

private:
	std::vector<std::unique_ptr<ImKeybind>> m_oKeybinds;


	size_t m_uSelectedPlayback = {};
};
