#pragma once

#include "r/gui/r_gui.hpp"

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


	size_t m_uSelectedPlayback = {};
};
