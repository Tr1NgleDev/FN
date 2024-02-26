//#define DEBUG_CONSOLE // Uncomment this if you want a debug console

#include <4dm.h>
using namespace fdm;

#include "4DKeyBinds.h"

initDLL

bool drawHud = true;

$hook(void, Player, renderHud, GLFWwindow* window)
{
	if(drawHud)
		original(self, window);
}

void toggleHud(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;
	drawHud = !drawHud;
	StateGame::instanceObj->crosshairRenderer.setColor(1.f, 1.f, 1.f, (drawHud ? 1.f : 0.f));
}

$hook(void, StateGame, keyInput, StateManager& s, int key, int scancode, int action, int mods)
{
	if (!KeyBinds::isLoaded() && key == GLFW_KEY_F1 && action == GLFW_PRESS)
		toggleHud(s.window, action, mods);

	original(self, s, key, scancode, action, mods);
}

$hook(void, StateIntro, init, StateManager& s)
{
	original(self, s);

	// initialize opengl stuff
	glewExperimental = true;
	glewInit();
	glfwInit();
}

$exec
{
	KeyBinds::addBind("F1", "Toggle Hud", glfw::Keys::F1, KeyBindsScope::STATEGAME, toggleHud);
}
