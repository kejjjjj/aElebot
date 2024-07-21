#include "r/r_drawactive.hpp"

#if(DEBUG_SUPPORT)
#include "cg/cg_local.hpp"
#include "cg/cg_offsets.hpp"
#include "cod4x/cod4x.hpp"
#include "main.hpp"
#include "r/gui/r_main_gui.hpp"
#include "r/r_utils.hpp"
#include "utils/hook.hpp"

long __stdcall R_EndScene(IDirect3DDevice9* device)
{
	if (R_NoRender())
		return hooktable::find<long, IDirect3DDevice9*>(HOOK_PREFIX(__func__))->call(device);

	auto MainGui = CStaticMainGui::Owner.get();

	if (MainGui && MainGui->OnFrameBegin()) {
		MainGui->Render();

		MainGui->OnFrameEnd();
	}

	return hooktable::find<long, IDirect3DDevice9*>(HOOK_PREFIX(__func__))->call(device);
}
#else
long __stdcall R_EndScene([[maybe_unused]]IDirect3DDevice9* device)
{
	
	return 0;
}
#endif