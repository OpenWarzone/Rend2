#include "dock_julia.h"
#include "../include_imgui.h"
//#include <client/client.h>

DockJulia::DockJulia() {
}

const char *DockJulia::label() {
	return "Julia";
}

void DockJulia::imgui() {
	ImGui::Button("DockJulia");
}