//#define DEBUG_CONSOLE // Uncomment this if you want a debug console

#include <4dm.h>
using namespace fdm;

#include "psapi.h"

#include <glm/gtc/random.hpp>

#include "ilerp.h"

#include "4DKeyBinds.h"

initDLL

enum ViewMode
{
	FIRST_PERSON,
	THIRD_PERSON_BACK,
	THIRD_PERSON_FRONT,

	VIEW_MODES_COUNT,

	FREECAM
};

bool drawHud = true;
bool debugInfo = false;
bool chunkBorders = false;
ViewMode viewMode = FIRST_PERSON;

struct
{
	glm::vec4 pos;
	glm::vec4 vel;

	m4::Mat5 orientation{ 1 };
	glm::vec4 left{ 1,0,0,0 };
	glm::vec4 up{ 0,1,0,0 };
	glm::vec4 forward{ 0,0,1,0 };
	glm::vec4 over{ 0,0,0,1 };

	bool inControl = true;

	float speed = 60.0f;
} freecam;

/*
* wip mod support
typedef decltype(Item::renderEntity) renderEntity_t;
void renderEntity_empty(Item* self, const m4::Mat5& MV, bool inHand, const glm::vec4& lightDir)
{

}

template<class T, typename FuncType>
FuncType overrideVFunc(T* obj, size_t i, FuncType newFunc)
{
	void** vftable = *(void***)obj;

	FuncType og = reinterpret_cast<FuncType>(vftable[i]);

	DWORD oldProtect;
	VirtualProtect(&vftable[i], sizeof(void*), PAGE_READWRITE, &oldProtect);

	vftable[i] = reinterpret_cast<void*>(newFunc);
	
	VirtualProtect(&vftable[i], sizeof(void*), oldProtect, NULL);

	return og;
}
*/

inline static constexpr float easeOutExpo(float x)
{
	return x == 1.0f ? 1.0f : 1.0f - pow(2.0f, -10.0f * x);
}

inline static double dt = 0.01;
inline static double realDT = 0.01;
inline static double averageFPS = 0;
inline static float debugInfoTime = 0;

inline static constexpr glm::vec4 red{
	240 / 255.0f, 79 / 255.0f, 67 / 255.0f, 1
};
inline static constexpr glm::vec4 green{
	27 / 255.0f, 222 / 255.0f, 79 / 255.0f, 1
};
inline static constexpr glm::vec4 yellow{
	200 / 255.0f, 224 / 255.0f, 61 / 255.0f, 1
};
inline static constexpr glm::vec4 orange{
	217 / 255.0f, 140 / 255.0f, 46 / 255.0f, 1
};
inline static constexpr glm::vec4 cyan{
	121 / 255.0f, 214 / 255.0f, 248 / 255.0f, 1
};
inline static constexpr glm::vec4 magenta{
	242 / 255.0f, 89 / 255.0f, 191 / 255.0f, 1
};
inline static constexpr glm::vec4 blue{
	72 / 255.0f, 114 / 255.0f, 219 / 255.0f, 1
};
inline static constexpr glm::vec4 white{
	1, 1, 1, 1
};
inline static constexpr glm::vec4 lightGray{
	0.85, 0.85, 0.85, 1
};

glm::i64vec3 getChunkPos(const glm::ivec4& pos)
{
	return glm::i64vec3
	{
		pos.x >= 0 ? pos.x / 8 : pos.x / 8 - 1,
		pos.z >= 0 ? pos.z / 8 : pos.z / 8 - 1,
		pos.w >= 0 ? pos.w / 8 : pos.w / 8 - 1,
	};
}

glm::ivec4 getChunkRelative(const glm::ivec4& pos)
{
	return glm::ivec4
	{
		pos.x >= 0 ? pos.x % 8 : ((8 * pos.x / 8 + 8) + pos.x) % 8,
		pos.y,
		pos.z >= 0 ? pos.z % 8 : ((8 * pos.z / 8 + 8) + pos.z) % 8,
		pos.w >= 0 ? pos.w % 8 : ((8 * pos.w / 8 + 8) + pos.w) % 8,
	};
}

inline static size_t getMemoryTotal()
{
	MEMORYSTATUSEX memInfo;
	memInfo.dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(&memInfo);

	return memInfo.ullTotalPhys;
}

inline static size_t getMemoryUsage()
{
	HANDLE proc = GetCurrentProcess();

	PROCESS_MEMORY_COUNTERS_EX pmc;
	GetProcessMemoryInfo(proc, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));

	size_t swapMemUsed = pmc.PagefileUsage;
	size_t physMemUsed = pmc.WorkingSetSize;

	return swapMemUsed + physMemUsed;
}

inline static Entity* intersectedEntity = nullptr;
$hook(void, Player, updateTargetBlock, World* world, float maxDist)
{
	original(self, world, maxDist);

	intersectedEntity = world->getEntityIntersection(self->cameraPos, self->reachEndpoint, self->EntityPlayerID);
}

void renderDebugInfo(Player* player)
{
	if (debugInfo)
		debugInfoTime += dt;

	FontRenderer& font = StateGame::instanceObj->font;
	QuadRenderer& qr = StateGame::instanceObj->qr;
	World* world = StateGame::instanceObj->world.get();
	ChunkLoader* chunkLoader = nullptr;
	if (world->getType() == World::TYPE_SINGLEPLAYER)
		chunkLoader = &((WorldSingleplayer*)world)->chunkLoader;

	glDepthMask(0);
	glDisable(GL_DEPTH_TEST);

	static int w = 0;
	static int h = 0;
	static int lastW = 0;
	static int lastH = 0;
	bool shaky = false;

	static auto text = [&](const std::string& text, const glm::vec4& color, const glm::ivec2& pos)
	{
		font.fontSize = 1;
		glm::ivec2 posA
		{
			(int)lerp(-100.0f, pos.x * (8 * font.fontSize) + 5, easeOutExpo(glm::clamp(debugInfoTime / 1.0f, 0.0f, 1.0f))),
			pos.y * (9 * font.fontSize) + 5
		};
		if (shaky)
		{
			posA += glm::circularRand(2);
		}
		font.centered = false;
		font.setText(text);
		// shadow
		font.color = { 0,0,0,0.2 };
		font.pos = posA + glm::ivec2{ 1, 1 };
		font.updateModel();
		font.render();
		// text
		font.color = color;
		font.pos = posA;
		font.updateModel();
		font.render();

		w = glm::max(w, posA.x + font.fontSize * 8 * (int)text.size());
		h = glm::max(h, posA.y + font.fontSize * 10);
	};

	w = 0;
	h = 0;

	if (debugInfoTime >= 0)
	{
		qr.setQuadRendererMode(GL_TRIANGLES); // <----------------------------------------------------------------------------------------------------------------------------- TR1NGLE REFERENCE
		qr.setColor(0, 0, 0, 0.5);
		qr.setPos(0, 0, lastW + 5, lastH + 5);
		qr.render();

		int line = -1;
		// 4D Miner VERSION
		{
			++line;

			text("4", magenta, { 0,line });
			text("D", cyan, { 1,line });
			text("Miner", lightGray, { 3,line });
			text(StateTitleScreen::instanceObj->versionText.text, white, { 9,line });
		}
		++line;

		// FPS: fps/fpsAverage
		{
			++line;

			text("FPS:", lightGray, { 0,line });
			static glm::vec4 color = red;
			if (averageFPS <= 20)
				color = ilerp(color, red, 0.1f, dt);
			else if (averageFPS <= 59)
				color = ilerp(color, orange, 0.1f, dt);
			else if (averageFPS >= 60)
				color = ilerp(color, green, 0.1f, dt);
			text(
				averageFPS <= 4 ? "skill issue" : 
				std::format("{} ({})", (int)(1.0 / realDT), (int)averageFPS),
				color, {5,line});
		}
		++line;

		// positions
		if (player->isHoldingCompass())
		{
			{
				++line;

				text("Position:", orange, { 0,line });
				std::string x = std::format("X:{:+.2f}", player->pos.x);
				std::string y = std::format("Y:{:+.2f}", player->pos.y);
				std::string z = std::format("Z:{:+.2f}", player->pos.z);
				std::string w = std::format("W:{:+.2f}", player->pos.w);
				++line;

				text(x, red, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, white, { 4 + x.size() + y.size() + z.size() + 3, line });
			}
			{
				++line;

				text("Block:", orange, { 0,line });
				std::string x = std::format("X:{:+}", player->currentBlock.x);
				std::string y = std::format("Y:{:+}", player->currentBlock.y);
				std::string z = std::format("Z:{:+}", player->currentBlock.z);
				std::string w = std::format("W:{:+}", player->currentBlock.w);
				++line;

				text(x, red, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, white, { 4 + x.size() + y.size() + z.size() + 3, line });
			}
			{
				++line;

				text("Chunk:", orange, { 0,line });
				auto chunkPos = getChunkPos(player->currentBlock);
				std::string x = std::format("X:{:+}", chunkPos.x);
				std::string z = std::format("Z:{:+}", chunkPos.y);
				std::string w = std::format("W:{:+}", chunkPos.z);
				++line;

				text(x, red, { 4, line });
				text(z, blue, { 4 + x.size() + 1, line });
				text(w, white, { 4 + x.size() + z.size() + 2, line });
			}
			{
				++line;

				text("Chunk Relative:", orange, { 0,line });
				auto chunkRelative = getChunkRelative(player->currentBlock);
				std::string x = std::format("X:{:+}", chunkRelative.x);
				std::string y = std::format("Y:{:+}", chunkRelative.y);
				std::string z = std::format("Z:{:+}", chunkRelative.z);
				std::string w = std::format("W:{:+}", chunkRelative.w);
				++line;

				text(x, red, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, white, { 4 + x.size() + y.size() + z.size() + 3, line });
			}
			++line;
		}
		// orientation
		{
			{
				++line;

				text("Facing:", orange, { 0,line });
				std::string x = std::format("X:{:+.2f}", player->forward.x);
				std::string y = std::format("Y:{:+.2f}", player->forward.y);
				std::string z = std::format("Z:{:+.2f}", player->forward.z);
				std::string w = std::format("W:{:+.2f}", player->forward.w);
				++line;

				text(x, red, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, white, { 4 + x.size() + y.size() + z.size() + 3, line });
			}
			{
				++line;

				text("Rotation:", orange, { 0,line });
				std::string x = std::format("XZ:{:+.2f}", glm::degrees(glm::asin(player->left.z)));
				std::string y = std::format("YZ:{:+.2f}", glm::degrees(glm::asin(player->forward.y)));
				std::string z = std::format("XW:{:+.2f}", glm::degrees(glm::asin(player->left.w)));
				std::string w = std::format("ZW:{:+.2f}", glm::degrees(glm::asin(player->forward.w)));
				++line;

				text(x, green, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, blue, { 4 + x.size() + y.size() + z.size() + 3, line });
			}
		}
		++line;

		// biome
		if (chunkLoader)
		{
			++line;
			auto biome = chunkLoader->getBiome(player->currentBlock.x, player->currentBlock.z, player->currentBlock.w);
			text("Biome:", yellow, { 0, line });
			text(chunkLoader->biomes[biome.biome].name, green, {7, line});
		}
		// entities
		{
			++line;
			text("Entities:", yellow, { 0, line });
			text(std::format("{}", world->entities.entities.size() - world->entities.blockEntities.size()), green, { 10, line });
		}
		++line;

		// mem
		{
			++line;
			
			text("Memory Usage:", red, { 0, line });

			size_t memUsed = getMemoryUsage();
			size_t memTotal = getMemoryTotal();
			double p = (double)memUsed / (double)memTotal * 100.0;

			static glm::vec4 color = green;
			if (p > 100)
			{
				color = ilerp(color, red, 0.1f, dt);
				shaky = true;
			}
			else if (p > 70)
				color = ilerp(color, red, 0.1f, dt);
			else if (p > 20)
				color = ilerp(color, orange, 0.1f, dt);
			else
				color = ilerp(color, green, 0.1f, dt);

			std::string used = std::format("{}MB", (uint32_t)(memUsed / 1024.0 / 1024.0));
			std::string total = std::format("{}MB", (uint32_t)(memTotal / 1024.0 / 1024.0));
			std::string percentage = std::format("({:.2f}%)", p);

			text(used, color, { 14, line });
			text("/", white, { 14 + used.size(), line });
			text(total, magenta, { 14 + used.size() + 1, line });
			text(percentage, color, { 14 + used.size() + total.size() + 2, line });

			shaky = false;
		}
		++line;

		// target block
		if (player->targetingBlock)
		{
			++line;

			text("Targeted Block:", blue, { 0, line });
			++line;

			if (player->isHoldingCompass())
			{
				std::string x = std::format("X:{:+}", player->targetBlock.x);
				std::string y = std::format("Y:{:+}", player->targetBlock.y);
				std::string z = std::format("Z:{:+}", player->targetBlock.z);
				std::string w = std::format("W:{:+}", player->targetBlock.w);
				text(x, red, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, white, { 4 + x.size() + y.size() + z.size() + 3, line });
				++line;
			}

			text("ID:", yellow, { 4, line });
			uint8_t block = world->getBlock(player->targetBlock);
			std::string name = "Unknown";
			if (BlockInfo::blockNames->contains(block))
				name = BlockInfo::blockNames->at(block);
			text(std::format("{} ({})", (int)block, name), white, {4 + 4, line});
		}
		++line;
		// target entity
		if (intersectedEntity)
		{
			++line;

			text("Targeted Entity:", blue, { 0, line });
			++line;

			if (player->isHoldingCompass())
			{
				glm::vec4 entityPos = intersectedEntity->getPos();
				std::string x = std::format("X:{:+.2f}", entityPos.x);
				std::string y = std::format("Y:{:+.2f}", entityPos.y);
				std::string z = std::format("Z:{:+.2f}", entityPos.z);
				std::string w = std::format("W:{:+.2f}", entityPos.w);
				text(x, red, { 4, line });
				text(y, green, { 4 + x.size() + 1, line });
				text(z, blue, { 4 + x.size() + y.size() + 2, line });
				text(w, white, { 4 + x.size() + y.size() + z.size() + 3, line });
				++line;
			}

			text("Name:", yellow, { 4, line });
			std::string name = intersectedEntity->getName();
			text(name, white, { 4 + 6, line });
		}

		if (!debugInfo)
			debugInfoTime -= dt;

		shaky = false;
	}

	lastW = w;
	lastH = h;

	glDepthMask(1);
	glEnable(GL_DEPTH_TEST);
}

$hook(void, Player, renderHud, GLFWwindow* window)
{
	if (drawHud)
	{
		if (viewMode != FIRST_PERSON)
		{
			glDepthMask(0);

			int wW, wH;
			glfwGetWindowSize(window, &wW, &wH);

			if (self->inventoryManager.isOpen())
			{
				double cursorX, cursorY;
				glfwGetCursorPos(window, &cursorX, &cursorY);
				glm::ivec2 cursor{ cursorX, cursorY };
				self->inventoryManager.ui.render();
				self->inventoryManager.secondary->render(cursor);
				self->inventoryManager.primary->render(cursor);
				self->inventoryManager.cursor.pos = cursor;
				if (self->inventoryManager.cursor.item)
				{
					Item::renderItem(self->inventoryManager.cursor.item, { cursorX - 20, cursorY - 20 });
				}
			}
			else
			{
				self->hotbar.render({ -1, -1 });
				self->equipment.render({ -1, -1 });
			}

			TexRenderer& healthRenderer = *self->healthRenderer;

			int h = (int)(self->health / 100.f * 48.f);
			healthRenderer.setClip(0, 48 - h, 32, h);
			healthRenderer.setPos(10, wH - h * 2 - 10, 64, h * 2);
			healthRenderer.render();

			healthRenderer.setClip(32, 0, 32, 48);
			if (glfwGetTime() - self->damageTime < Player::DAMAGE_COOLDOWN)
			{
				glm::vec2 a = glm::diskRand(2.f);
				glm::vec2 b = glm::diskRand(2.f);

				healthRenderer.setColor(1, 0, 1, 1);
				healthRenderer.setPos(a.x + 10, a.y + wH - 106, 64, 96);
				healthRenderer.render();

				healthRenderer.setColor(0, 1, 1, 1);
				healthRenderer.setPos(b.x + 10, b.y + wH - 106, 64, 96);
				healthRenderer.render();

				healthRenderer.setColor(1, 1, 1, 1);
			}

			healthRenderer.setPos(10, wH - 106, 64, 96);
			healthRenderer.render();

			glDepthMask(1);

			renderDebugInfo(self);

			return;
		}
		/*
		* wip mod support
		if (viewMode != FIRST_PERSON)
		{
			Item* a = self->hotbar.getSlot(self->hotbar.selectedIndex)->get();
			Item* b = self->equipment.getSlot(0)->get();
			auto ogA = overrideVFunc(a, 3, renderEntity_empty);
			auto ogB = overrideVFunc(b, 3, renderEntity_empty);

			original(self, window);

			overrideVFunc<Item, renderEntity_t>(a, 3, ogA, nullptr);

			return;
		}
		*/
		original(self, window);

		renderDebugInfo(self);
	}
}

StateManager* stateManager = nullptr;

void toggleHud(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	drawHud = !drawHud;

	StateGame::instanceObj->crosshairRenderer.setColor(1.f, 1.f, 1.f, (drawHud ? 1.f : 0.f));
}
void toggleViewMode(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	Player& player = StateGame::instanceObj->player;

	viewMode = (ViewMode)(((int)viewMode + 1) % VIEW_MODES_COUNT);

	player.updateAllComponentVectors();
	player.updateCameraPos(StateGame::instanceObj->world.get());
}
void toggleFreeCam(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	Player& player = StateGame::instanceObj->player;

	if (viewMode == FREECAM)
	{
		viewMode = FIRST_PERSON;
	}
	else
	{
		viewMode = FREECAM;
		freecam.inControl = true;
		freecam.pos = player.cameraPos;
		freecam.left = player.left;
		freecam.up = player.up;
		freecam.forward = player.forward;
		freecam.over = player.over;
	}

	player.updateAllComponentVectors();
	player.updateCameraPos(StateGame::instanceObj->world.get());
}
void toggleFreeCamControl(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	Player& player = StateGame::instanceObj->player;

	freecam.inControl = !freecam.inControl;

	player.updateAllComponentVectors();
	player.updateCameraPos(StateGame::instanceObj->world.get());
}
void toggleDebugInfo(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	debugInfo = !debugInfo;
	debugInfoTime = debugInfo ? 0 : 1;
}
void reloadChunks(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	StateGame& stateGame = *StateGame::instanceObj;
	stateGame.player.hyperplaneUpdateFlag = true;
	stateGame.world->updateLocal(*stateManager, &stateGame.player, 0);
}
void toggleFullscreen(GLFWwindow* window, int action, int mods)
{
	if (action != GLFW_PRESS)
		return;

	StateSettings::instanceObj->setFullscreenMode(window, !StateSettings::instanceObj->fullscreen);
}

$hook(void, StateGame, keyInput, StateManager& s, int key, int scancode, int action, int mods)
{
	if (!KeyBinds::isLoaded() && action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_F1:
			toggleHud(s.window, action, mods);
			break;
		case GLFW_KEY_F3:
			toggleDebugInfo(s.window, action, mods);
			break;
		case GLFW_KEY_F5:
			toggleViewMode(s.window, action, mods);
			break;
		case GLFW_KEY_F6:
			toggleFreeCam(s.window, action, mods);
			break;
		case GLFW_KEY_P:
			toggleFreeCamControl(s.window, action, mods);
			break;
		case GLFW_KEY_F8:
			reloadChunks(s.window, action, mods);
			break;
		case GLFW_KEY_F11:
			toggleFullscreen(s.window, action, mods);
			break;
		}
	}

	original(self, s, key, scancode, action, mods);
}

$hook(void, StateIntro, init, StateManager& s)
{
	original(self, s);

	// initialize opengl stuff
	glewExperimental = true;
	glewInit();
	glfwInit();

	stateManager = &s;
}

$hook(void, Player, update, World* world, double dt, EntityPlayer* entityPlayer)
{
	if (self != &StateGame::instanceObj->player) return;

	bool w = self->keys.w;
	bool s = self->keys.s;
	bool a = self->keys.a;
	bool d = self->keys.d;
	bool q = self->keys.q;
	bool e = self->keys.e;
	bool shift = self->keys.shift;
	bool ctrl = self->keys.ctrl;
	bool leftMouseDown = self->keys.leftMouseDown;
	bool rightMouseDown = self->keys.rightMouseDown;
	bool space = self->keys.space;

	if (viewMode == FREECAM && freecam.inControl)
	{
		float speed = freecam.speed * (self->keys.ctrl ? 2.0f : 1.0f);
		if (self->keys.w)
			freecam.vel += freecam.forward * (float)dt * speed;
		if (self->keys.s)
			freecam.vel -= freecam.forward * (float)dt * speed;
		if (self->keys.a)
			freecam.vel += freecam.left * (float)dt * speed;
		if (self->keys.d)
			freecam.vel -= freecam.left * (float)dt * speed;
		if (self->keys.q && self->keys.shift)
			freecam.vel += freecam.over * (float)dt * speed;
		if (self->keys.e && self->keys.shift)
			freecam.vel -= freecam.over * (float)dt * speed;
		if (self->keys.space)
			freecam.vel += freecam.up * (float)dt * speed;
		if (!self->keys.q && !self->keys.e && self->keys.shift)
			freecam.vel -= freecam.up * (float)dt * speed;
		self->keys.w = false;
		self->keys.a = false;
		self->keys.s = false;
		self->keys.d = false;
		self->keys.q = false;
		self->keys.e = false;
		self->keys.shift = false;
		self->keys.ctrl = false;
		self->keys.leftMouseDown = false;
		self->keys.rightMouseDown = false;
		self->keys.space = false;
	}

	freecam.pos += freecam.vel * (float)dt;

	freecam.vel.x = ilerp(freecam.vel.x, 0, 0.15f, dt);
	freecam.vel.y = ilerp(freecam.vel.y, 0, 0.15f, dt);
	freecam.vel.z = ilerp(freecam.vel.z, 0, 0.15f, dt);
	freecam.vel.w = ilerp(freecam.vel.w, 0, 0.15f, dt);

	// reverse input if looking at self real
	if (viewMode == THIRD_PERSON_FRONT)
	{
		self->keys.w = s;
		self->keys.s = w;
		self->keys.a = d;
		self->keys.d = a;
	}

	original(self, world, dt, entityPlayer);

	self->keys.w = w;
	self->keys.s = s;
	self->keys.a = a;
	self->keys.d = d;
	self->keys.q = q;
	self->keys.e = e;
	self->keys.shift = shift;
	self->keys.ctrl = ctrl;
	self->keys.leftMouseDown = leftMouseDown;
	self->keys.rightMouseDown = rightMouseDown;
	self->keys.space = space;
}

$hook(void, EntityPlayer, setPos, const glm::vec4& pos)
{
	Player& player = StateGame::instanceObj->player;

	if (&player != self->ownedPlayer.get()) return original(self, pos);

	return;
}

static Player* entityPlayerOwnedPlayer = nullptr;
$hook(void, EntityPlayer, update, World* world, double dt)
{
	Player& player = StateGame::instanceObj->player;
	
	if (self->id != player.EntityPlayerID) return original(self, world, dt);

	entityPlayerOwnedPlayer = *(Player**)&self->ownedPlayer;
	float targetDamage = player.targetDamage;
	double leftClickActionTime = player.leftClickActionTime;
	double rightClickActionTime = player.rightClickActionTime;
	double damageTime = player.damageTime;
	decltype(Player::keys) keys = player.keys;
	float health = player.health;
	double mouseDownTime = player.mouseDownTime;
	double walkStartTime = player.walkStartTime;
	float walkAnimOffset = player.walkAnimOffset;
	float walkAnimTheta = player.walkAnimTheta;
	double mineStartTime = player.mineStartTime;
	float mineAnimTheta = player.mineAnimTheta;
	bool crouching = player.crouching;
	bool sprinting = player.sprinting;
	bool walking = player.walking;

	*(Player**)&self->ownedPlayer = &player;
	self->lastMovementUpdateTime = glfwGetTime();
	original(self, world, dt);

	player.targetDamage = targetDamage;
	player.leftClickActionTime = leftClickActionTime;
	player.rightClickActionTime = rightClickActionTime;
	player.damageTime = damageTime;
	player.keys = keys;
	player.mouseDownTime = mouseDownTime;
	player.health = health;
	player.walkStartTime = walkStartTime;
	player.walkAnimOffset = walkAnimOffset;
	player.walkAnimTheta = walkAnimTheta;
	player.mineStartTime = mineStartTime;
	player.mineAnimTheta = mineAnimTheta;
	player.crouching = crouching;
	player.sprinting = sprinting;
	player.walking = walking;

	*(Player**)&self->ownedPlayer = nullptr;
	original(self, world, 0);
}

$hook(void, StateGame, render, StateManager& s)
{
	static double lastTime = glfwGetTime() - 0.01;
	double curTime = glfwGetTime();
	realDT = curTime - lastTime;
	lastTime = curTime;

	static double aDT = 0;
	static int aDTi = 0;
	aDT += realDT;
	++aDTi;
	if (aDT >= 1)
	{
		averageFPS = 1.0 / (aDT / aDTi);
		aDT = 0;
		aDTi = 0;
	}

	dt = glm::min(realDT, 0.1);

	static stl::string loadedSkin = "";

	Entity* entity = self->world->entities.get(self->player.EntityPlayerID);
	EntityPlayer* entityPlayer = nullptr;

	if (!entity)
		goto doOriginal;

	entityPlayer = (EntityPlayer*)entity;
	if (viewMode != FIRST_PERSON)
	{
		*(Player**)&entityPlayer->ownedPlayer = &self->player;

		if (loadedSkin != StateSkinChooser::instanceObj->skinPath && !StateSkinChooser::instanceObj->skinPath.empty())
		{
			loadedSkin = StateSkinChooser::instanceObj->skinPath;
			entityPlayer->skin.load(StateSkinChooser::instanceObj->skinPath);
			entityPlayer->skinRenderer.skin = &entityPlayer->skin;
		}
	}

doOriginal:

	m4::Mat5 playerOrientation = self->player.orientation;
	glm::vec4 playerLeft = self->player.left;
	glm::vec4 playerUp = self->player.up;
	glm::vec4 playerForward = self->player.forward;
	glm::vec4 playerOver = self->player.over;
	glm::vec4 playerCamPos = self->player.cameraPos;

	if (viewMode == FREECAM)
	{
		self->player.orientation = freecam.orientation;
		self->player.left = freecam.left;
		self->player.up = freecam.up;
		self->player.forward = freecam.forward;
		self->player.over = freecam.over;
		self->player.cameraPos = freecam.pos;
	}

	original(self, s);

	self->player.orientation = playerOrientation;
	self->player.left = playerLeft;
	self->player.up = playerUp;
	self->player.forward = playerForward;
	self->player.over = playerOver;
	self->player.cameraPos = playerCamPos;

	if (entity)
		*(Player**)&entityPlayer->ownedPlayer = entityPlayerOwnedPlayer;
}

$hook(void, Player, updateComponentVectors)
{
	if (viewMode == FREECAM)
	{
		if (freecam.inControl)
		{
			//glm::vec4
			//	oldLeft = self->left,
			//	oldUp = self->up,
			//	oldForward = self->forward,
			//	oldOver = self->over;
			//m4::Mat5 oldOrientation = self->orientation;

			original(self);

			freecam.orientation = self->orientation;
			freecam.left = self->left;
			freecam.up = self->up;
			freecam.forward = self->forward;
			freecam.over = self->over;

			//self->orientation = oldOrientation;
			//self->left = oldLeft;
			//self->up = oldUp;
			//self->forward = oldForward;
			//self->over = oldOver;

			return;
		}
		else
			return original(self);
	}
	if (viewMode != THIRD_PERSON_FRONT)
		return original(self);

	m4::Mat5 orientation = self->orientation;
	m4::Rotor rot = m4::Rotor(m4::wedge({ 1,0,0,0 }, { 0,0,1,0 }), glm::pi<float>());
	//rot *= m4::Rotor(m4::wedge({ 0,1,0,0 }, { 0,0,1,0 }), glm::pi<float>());
	self->orientation *= m4::Mat5(rot);
	original(self);
	self->orientation = orientation;
}

$hook(void, Player, updateCameraPos, World* world)
{
	switch (viewMode)
	{
	case FREECAM:
	case FIRST_PERSON:
	{
		original(self, world);
		// nothing here
	} break;
	case THIRD_PERSON_FRONT:
	case THIRD_PERSON_BACK:
	{
		original(self, world);
		
		glm::vec4 curPos = self->cameraPos;
		glm::ivec4 curBlockPos = curPos;
		glm::vec4 endpoint = curPos - self->forward * 2.5f;
		glm::ivec4 intersectBlock = endpoint;
		bool hit = world->castRay(curPos, curBlockPos, intersectBlock, endpoint);
		if (hit)
		{
			curPos += self->forward * 0.05f;
		}
		self->cameraPos = curPos;
		
	} break;
	}
}

$hook(void, StateTitleScreen, init, StateManager& s)
{
	original(self, s);
	if (!self->multiplayerButton.clickable)
	{
		self->multiplayerButton.clickable = true;
		self->multiplayerButton.setText("Choose Skin");
		self->multiplayerButton.callback = StateMultiplayer::changeSkinButtonCallback;
		self->multiplayerButton.user = &s;
	}
}

$hook(void, StateGame, init, StateManager& s)
{
	// default the stuff
	drawHud = true;
	debugInfo = false;
	viewMode = FIRST_PERSON;
	freecam.inControl = true;

	original(self, s);
}

$exec
{
	KeyBinds::addBind("FN", "Toggle HUD",			glfw::Keys::F1, KeyBindsScope::STATEGAME, toggleHud);
	KeyBinds::addBind("FN", "Debug Info",			glfw::Keys::F3, KeyBindsScope::STATEGAME, toggleDebugInfo);
	KeyBinds::addBind("FN", "Third-Person View",	glfw::Keys::F5, KeyBindsScope::STATEGAME, toggleViewMode);
	KeyBinds::addBind("FN", "Freecam",				glfw::Keys::F6, KeyBindsScope::STATEGAME, toggleFreeCam);
	KeyBinds::addBind("FN", "Freecam Control",		glfw::Keys::P, KeyBindsScope::STATEGAME, toggleFreeCamControl);
	KeyBinds::addBind("FN", "Reload Chunks",		glfw::Keys::F8, KeyBindsScope::STATEGAME, reloadChunks);
	KeyBinds::addBind("FN", "Fullscreen",			glfw::Keys::F11, KeyBindsScope::STATEGAME, toggleFullscreen);
}

$hook(bool, Player, isHoldingCompass)
{
	return original(self) || (self->equipment.getSlot(0)->get() && self->equipment.getSlot(0)->get()->getName() == "Compass");
}
