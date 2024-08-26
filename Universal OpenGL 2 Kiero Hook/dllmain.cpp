#include <Windows.h>
#include "include/imgui_hook.h"
#include "include/imgui/imgui.h"
#include "include/kiero/kiero.h"
#include "minmem.h"

Mem gzdoom = Mem("gzdoom.exe", "gzdoom.exe");
int *health = reinterpret_cast<int*>(gzdoom.moduleBase + 0x1234F18);
int *monsters_killed = reinterpret_cast<int*>(gzdoom.moduleBase + 0x1238494);
bool opposite_damage = 0;

void RenderMain()
{
	ImGui::Begin("aceinetx.gzdoom");
	if (ImGui::IsWindowCollapsed()) {
		ImGui::GetIO().WantCaptureMouse = false;
	}
	ImGui::SliderInt("Health", health, 0, 255);
	ImGui::InputInt("Monsters killed", monsters_killed);
	if (ImGui::Checkbox("Opposite damage (godmode)", &opposite_damage)) {
		if (!opposite_damage) {
			gzdoom.PatchBytes(gzdoom.moduleBase + 0x2542AD, (BYTE*)"\x44\x29\xBA\x98\x00\x00\x00", 7);
		}
		else {
			gzdoom.PatchBytes(gzdoom.moduleBase + 0x2542AD, (BYTE*)"\x44\x01\xBA\x98\x00\x00\x00", 7);
		}
	}

	if (ImGui::Button("Uninject")) {
		ImGui::GetIO().WantCaptureMouse = false;
		ImGuiHook::Unload();
	}
	ImGui::End();
}

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved)
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:

		DisableThreadLibraryCalls(hMod);
		ImGuiHook::Load(RenderMain);

		break;
	case DLL_PROCESS_DETACH:
		ImGuiHook::Unload();
		break;
	}
	return TRUE;
}