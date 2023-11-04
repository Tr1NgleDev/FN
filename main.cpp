//#define DEBUG_CONSOLE // Uncomment this if you want a debug console

// Mod Name. Make sure it matches the mod folder's name. Also don't forget to change the output DLL name in Project Properties->General->Target Name
#define MOD_NAME "F1"
#define MOD_VER "1.0"

#include <Windows.h>
#include <cstdio>
#include <4dm.h>
using namespace fdm;

bool drawHud = true;

void(__thiscall* Player_renderHud)(Player* self, GLFWwindow* window);
void __fastcall Player_renderHud_H(Player* self, GLFWwindow* window) 
{
	if(drawHud)
		Player_renderHud(self, window);
}

void(__thiscall* StateGame_keyInput)(StateGame* self, StateManager& s, int key, int scancode, int action, int mods);
void __fastcall StateGame_keyInput_H(StateGame* self, StateManager& s, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
	{
		drawHud = !drawHud;
		self->crosshairRenderer.setColor(1.f, 1.f, 1.f, (drawHud ? 1.f : 0.f));
	}
		
	return StateGame_keyInput(self, s, key, scancode, action, mods);
}

DWORD WINAPI Main_Thread(void* hModule)
{
	// Create console window if DEBUG_CONSOLE is defined
#ifdef DEBUG_CONSOLE
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
#endif
	
	glfwInit();
	glewInit();

	Hook(reinterpret_cast<void*>(FUNC_PLAYER_RENDERHUD), reinterpret_cast<void*>(&Player_renderHud_H), reinterpret_cast<void**>(&Player_renderHud));

	Hook(reinterpret_cast<void*>(FUNC_STATEGAME_KEYINPUT), reinterpret_cast<void*>(&StateGame_keyInput_H), reinterpret_cast<void**>(&StateGame_keyInput));

	EnableHook(0);
	return true;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD _reason, LPVOID lpReserved)
{
	if (_reason == DLL_PROCESS_ATTACH)
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Main_Thread, hModule, 0, NULL);
	return TRUE;
}
