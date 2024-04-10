// Header
#include "world_system.hpp"
#include "world_init.hpp"

// stlib
#include <cassert>

#include "physics_system.hpp"
#include "movement_system.hpp"
#include "drawing_system.hpp"
#include <ai_system.hpp>

// Game configuration
const size_t MAX_BOULDERS = 5;
const size_t MAX_BUG = 5;
const size_t BOULDER_DELAY_MS = 2000 * 3;
const size_t BUG_DELAY_MS = 5000 * 3;
const float FRICTION = 5.f;
const size_t SPIKE_DELAY_MS = 3500;

// Create the world
WorldSystem::WorldSystem()
	: next_boulder_spawn(0.f) {
	// Seeding rng with random device
	rng = std::default_random_engine(std::random_device()());
}

WorldSystem::~WorldSystem() {
	// Destroy music components
	if (background_music != nullptr)
		Mix_FreeMusic(background_music);
	if (dead_sound != nullptr)
		Mix_FreeChunk(dead_sound);
	if (checkpoint_sound != nullptr)
		Mix_FreeChunk(checkpoint_sound);
	if (level_win_sound != nullptr)
		Mix_FreeChunk(level_win_sound);
	if (ink_pickup_sound != nullptr)
		Mix_FreeChunk(ink_pickup_sound);
	Mix_CloseAudio();

	// Destroy all created components
	registry.clear_all_components();

	// Close the window
	glfwDestroyWindow(window);
}

// Debugging
namespace {
	void glfw_err_cb(int error, const char *desc) {
		fprintf(stderr, "%d: %s", error, desc);
	}
}


// World initialization
// Note, this has a lot of OpenGL specific things, could be moved to the renderer
GLFWwindow* WorldSystem::create_window() {
	///////////////////////////////////////
	// Initialize GLFW
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW");
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// If you are on Linux or Windows, you can change these 2 numbers to 4 and 3 and
	// enable the glDebugMessageCallback to have OpenGL catch your mistakes for you.
	// GLFW / OGL Initialization
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#if __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, 0);

	// Create the main window (for rendering, keyboard, and mouse input)
	window = glfwCreateWindow(window_width_px, window_height_px, "Pathfinder", nullptr, nullptr);
	if (window == nullptr) {
		fprintf(stderr, "Failed to glfwCreateWindow");
		return nullptr;
	}

	// Setting callbacks to member functions (that's why the redirect is needed)
	// Input is handled using GLFW, for more info see
	// http://www.glfw.org/docs/latest/input_guide.html
	glfwSetWindowUserPointer(window, this);
	auto key_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2, int _3) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_key(_0, _1, _2, _3); };
	auto cursor_pos_redirect = [](GLFWwindow* wnd, double _0, double _1) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_move({ _0, _1}); };
	auto mouse_button_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_click(_0, _1, _2);};
	glfwSetKeyCallback(window, key_redirect);
	glfwSetCursorPosCallback(window, cursor_pos_redirect);
	glfwSetMouseButtonCallback(window, mouse_button_redirect); 

	// Set cursor mode to hidden
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	//////////////////////////////////////
	// Loading music and sounds with SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL Audio");
		return nullptr;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
		fprintf(stderr, "Failed to open audio device");
		return nullptr;
	}

	background_music = Mix_LoadMUS(audio_path("music.wav").c_str());
	Mix_VolumeMusic(10);
	dead_sound = Mix_LoadWAV(audio_path("dead.wav").c_str());
	checkpoint_sound = Mix_LoadWAV(audio_path("checkpoint.wav").c_str());
	level_win_sound = Mix_LoadWAV(audio_path("level_win.wav").c_str());
	ink_pickup_sound = Mix_LoadWAV(audio_path("ink_pickup.wav").c_str());

	Mix_VolumeChunk(dead_sound, 10);
	Mix_VolumeChunk(checkpoint_sound, 5);
	Mix_VolumeChunk(level_win_sound, 10);
	Mix_VolumeChunk(ink_pickup_sound, 10);

	if (background_music == nullptr || dead_sound == nullptr || checkpoint_sound == nullptr || level_win_sound == nullptr || ink_pickup_sound == nullptr) {
		fprintf(stderr, "Failed to load sounds\n %s\n %s\n %s\n make sure the data directory is present",
			audio_path("music.wav").c_str(),
			audio_path("chicken_dead.wav").c_str(),
			audio_path("chicken_eat.wav").c_str());
		return nullptr;
	}

	return window;
}

void WorldSystem::init(RenderSystem* renderer_arg, bool* mainMenu) {
	WorldSystem::mainMenu = mainMenu;
	this->renderer = renderer_arg;
	// Playing background music indefinitely
	Mix_PlayMusic(background_music, -1);
	fprintf(stderr, "Loaded music\n");

	//init levels
	WorldSystem::level_idx = config.starting_level;
	LevelManager lm;
	lm.loadLevels();
	//lm.printLevelsInfo();
	this->levelManager = lm;
	// Set all states to default
    restart_game();
}

std::pair<float, float> advancedAIlerp(float x0, float y0, float x1, float y1, float t) {
	//printf("x0:%f\n", x0);
	//printf("x1:%f\n", x1);
	//printf("y0:%f\n", y0);
	//printf("y1:%f\n", y1);
	float x = x0 + t * (x1 - x0);
	float y = y0 + t * (y1 - y0);
	return { x, y };
}

void WorldSystem::switchHintAnimation(Entity e, float elapsedTime) {
	hintElapsedMsTotal += elapsedTime;
	float minMsChange = 20.f;
	if (hintElapsedMsTotal > minMsChange) {
		currentHintTexture += static_cast<int>(hintElapsedMsTotal / minMsChange);
		hintElapsedMsTotal = 0;
		if (currentHintTexture > (int)TEXTURE_ASSET_ID::HINT8) {
			currentHintTexture = (int)TEXTURE_ASSET_ID::HINT1;
		}
		registry.renderRequests.get(e).used_texture = static_cast<TEXTURE_ASSET_ID>(currentHintTexture);
	}
}

void drawLinesLoop(int startX, int gapX, int startY, int gapY, int n, float rotation, float gapR) {
	for (int i = 0; i < n; i++) {
		createLine({ startX, startY }, { 10, 20 }, rotation);
		startX += gapX;
		startY += gapY;
		rotation += gapR;
	}
}

void WorldSystem::createDrawOnLines(int x, int y, float rotation) {
	Entity e = createLine({ x, y }, { 10, 20 }, rotation);
	registry.toDrawOns.emplace(e);
}

void WorldSystem::createIndividualPlatforms(vec2 position, vec2 size) {
	createWall(renderer, position, size);
	int platformHeight = abs(position.y - window_height_px) + size.y / 2 + 2;
	createPlatform(renderer, {position.x, window_height_px - platformHeight }, {size.x - 10, 10.f });
}

void WorldSystem::handleDrawOnLines(int currDrawing) {
	drawings.stop_drawing();
	for (Entity e : registry.motions.entities) {
		if (!registry.platforms.has(e) && !registry.players.has(e) && !registry.walls.has(e) && !registry.levelEnds.has(e) &&
			!registry.checkpoints.has(e) && !registry.pencil.has(e) &&!registry.hints.has(e)) {
			registry.remove_all_components_of(e);
		}
	}
	if (currDrawing == 0) {
		drawLinesLoop(600, 0, 200, 25, 10, 0, 0);
		drawLinesLoop(620, 25, 195, 0, 10, M_PI / 2, 0);
		drawLinesLoop(865, 0, 200, 25, 10, 0, 0);
		drawLinesLoop(620, 25, 430, 0, 10, M_PI / 2,0 );

		//actual lines
		//createDrawOnLines(600, 200, 0);
		//createDrawOnLines(620, 195, M_PI / 2);
		//createDrawOnLines(865, 200, 0);
		//createDrawOnLines(620, 430, M_PI / 2);
	}
	else if (currDrawing == 1) {
		DrawingSystem::remainingDrawingCount = level.inkLimit;
		createIndividualPlatforms({ 600, window_height_px - 400 }, { 200, 100 });
		Entity e = createPaintCan(renderer, { 600, window_height_px - 450 }, { 25.f, 50.f }, 300);
		Motion& m = registry.motions.get(e);
		m.fixed = true;
		drawLinesLoop(700, 25, 400, -25, 10, 0.698132, 0);
		drawLinesLoop(950, 25, 175, 25, 10, 2.44346, 0);
		drawLinesLoop(724, 47, 410, 0, 10, M_PI / 2, 0);
	}
	else if (currDrawing == 2) {
		DrawingSystem::remainingDrawingCount = level.inkLimit;
		createIndividualPlatforms({ 850, window_height_px - 400 }, { 200, 100 });
		Entity e = createPaintCan(renderer, { 850, window_height_px - 450 }, { 25.f, 50.f }, 300);
		Motion& m = registry.motions.get(e);
		m.fixed = true;
		drawLinesLoop(950, 25, 100, 0, 7, 1.5708, 0);
		drawLinesLoop(950, 25, 250, 0, 7, 1.5708, 0);
		//drawLinesLoop(950, 25, 550, 0, 7, 1.5708, 0);

		//drawLinesLoop(940, 0, 260, 45, 7, 0, 0);
		//drawLinesLoop(1115, 0, 260, 45, 7, 0, 0);

		drawLinesLoop(875, 25, 160, -25, 3, 0.698132, 0);
		drawLinesLoop(875, 25, 180, 25, 3, 2.44346, 0);

		drawLinesLoop(1125, 25, 240, -25, 3, 0.698132, 0);
		drawLinesLoop(1125, 25, 110, 25, 3, 2.44346, 0);

		//drawLinesLoop(1125, 25, 540, -25, 3, 0.698132, 0);
		//drawLinesLoop(875, 25, 490, 25, 3, 2.44346, 0);

		//drawLinesLoop(870, 0, 200, 45, 7, 0, 0);
		//drawLinesLoop(1185, 0, 200, 45, 7, 0, 0);
	}
	else if (currDrawing == 3) {
		DrawingSystem::remainingDrawingCount = level.inkLimit;
		createIndividualPlatforms({ 1100, window_height_px - 400 }, { 200, 100 });
		Entity e = createPaintCan(renderer, { 1100, window_height_px - 450 }, { 25.f, 50.f }, 300);
		Motion& m = registry.motions.get(e);
		m.fixed = true;
		drawLinesLoop(750, 25, 420, -40, 10, 0.698132, 0);
		drawLinesLoop(980, 25, 55, 40, 10, 2.44346, 0);
		drawLinesLoop(770, 70, 425, 0, 7, M_PI / 2, 0);

		//drawLinesLoop(740, 0, 400, -30, 5, 0, 0);
		//drawLinesLoop(1210, 0, 400, -30, 5, 0, 0);

		//drawLinesLoop(740, 35, 250, -30, 7, 0.698132, 0);
		//drawLinesLoop(1000, 35, 75, 30, 7, 2.44346, 0);
	}
	else if (currDrawing == 4) {
		createIndividualPlatforms({ 1350, window_height_px - 400 }, { 200, 100 });
	}
}

bool WorldSystem::isLineCollisionsOn() {
	return level.lineCollisionOn;
}

// Update our game world
bool WorldSystem::step(float elapsed_ms_since_last_update) {
	// Remove debug info from the last step
	while (registry.debugComponents.entities.size() > 0)
	    registry.remove_all_components_of(registry.debugComponents.entities.back());

	// Removing out of screen entities
	auto& motions_registry = registry.motions;
	// Remove entities that leave the screen on the left side
	// Iterate backwards to be able to remove without unterfering with the next object to visit
	// (the containers exchange the last element with the current)
	for (int i = (int)motions_registry.components.size()-1; i>=0; --i) {
	    Motion& motion = motions_registry.components[i];
		if (motion.position.x + abs(motion.scale.x) < 0.f) {
			if(!registry.players.has(motions_registry.entities[i])) // don't remove the player
				registry.remove_all_components_of(motions_registry.entities[i]);
		}
		// remove deadlys if they leave bottom of the screen
		if (motion.position.y - abs(motion.scale.y) > window_height_px) {
			if (registry.deadlys.has(motions_registry.entities[i]))
				registry.remove_all_components_of(motions_registry.entities[i]);
		}
	}

	Motion& pmotion = registry.motions.get(player);
	//vec2 pPosition = pmotion.position;

	// if entity is player and below window screen
	if (pmotion.position.y - abs(pmotion.scale.y) / 2 > window_height_px) {
		if (!registry.deathTimers.has(player)) {
			registry.deathTimers.emplace(player);
			Mix_PlayChannel(-1, dead_sound, 0);
			if (drawings.currently_drawing())
				drawings.stop_drawing();
		}
	}

	next_boulder_spawn -= elapsed_ms_since_last_update * current_speed * 2;
	if (next_boulder_spawn < 0.f) {
		for (const InitBoulderSpawner& boulderSpawner : level.boulderSpawners) {
			if (registry.boulders.components.size() <= MAX_BOULDERS) {
				// Reset timer
				vec2 spawnpoint = boulderSpawner.pos;
				if (boulderSpawner.random_x) {
					spawnpoint.x = 50.f + uniform_dist(rng) * (window_width_px - 100.f);
				}
				next_boulder_spawn = (boulderSpawner.delay / 2) + uniform_dist(rng) * (boulderSpawner.delay / 2);
				createBoulder(renderer, spawnpoint);
			}
		}
		for (const InitSpikeProjectileSpawner& spikeProjectileSpawner : level.spikeProjectileSpawners) {
			next_boulder_spawn = (spikeProjectileSpawner.delay / 2) + uniform_dist(rng) * (spikeProjectileSpawner.delay / 2);
			Entity e = createSpikes(renderer, spikeProjectileSpawner.pos, spikeProjectileSpawner.size, spikeProjectileSpawner.angle);
			Motion& m = registry.motions.get(e);
			m.velocity = spikeProjectileSpawner.vel;
			m.fixed = false;
			m.gravityScale = 0.f;
		}
	}

	if(!registry.deathTimers.has(player) && level.hasChaseBoulder)
	{
		FrameCount += elapsed_ms_since_last_update;
		if (FrameCount >= FrameInterval) {
			aiSystem.updateGrid(levelManager.levels[level_idx].walls);
			//aiSystem.printGrid();
			Motion& eMotion = registry.motions.get(chaseBoulder);
			Motion& pMotion = registry.motions.get(player);
			bestPath = aiSystem.bestPath(eMotion, pMotion);
			currentNode = 0;
			FrameCount = 0;
		}

		Motion& eMotion = registry.motions.get(chaseBoulder);

		//std::cout << "Path found: ";
		//for (const auto& p : bestPath) {
		//	std::cout << "(" << p.first << ", " << p.second << ") ";

		//}
		//std::cout << std::endl;

		if (bestPath.size() != 0 && currentNode < bestPath.size() - 1) {
			float x0 = eMotion.position.x;
			float y0 = eMotion.position.y;
			float x1 = (bestPath[currentNode + 1].first + 1) * gridSize;
			float y1 = (bestPath[currentNode + 1].second + 1) * gridSize;

			if (debugging.in_debug_mode) {
				printf("x0:%f\n", x0);
				printf("x1:%f\n", x1);

				printf("y0:%f\n", y0);
				printf("y1:%f\n", y1);
			}

			if (x0 > x1) {
				x1 = (bestPath[currentNode + 1].first) * gridSize;
			}

			if (y0 > y1) {
				y1 = (bestPath[currentNode + 1].second) * gridSize;
			}

			float distance = std::sqrt(std::pow(x1 - x0, 2) + std::pow(y1 - y0, 2));
			//printf("distance:%f\n", distance);
			if (distance < 1) {
				currentNode++;
			}
			else {
				auto interpolatedPoint = advancedAIlerp(x0, y0, x1, y1, elapsed_ms_since_last_update / 1000.f);
				eMotion.position.x = interpolatedPoint.first;
				eMotion.position.y = interpolatedPoint.second;
			}
		}
	}

	assert(registry.screenStates.components.size() <= 1);
    ScreenState &screen = registry.screenStates.components[0];

    float min_counter_ms = 3000.f;
	for (Entity entity : registry.deathTimers.entities) {
		// progress timer
		DeathTimer& counter = registry.deathTimers.get(entity);
		counter.counter_ms -= elapsed_ms_since_last_update;
		if(counter.counter_ms < min_counter_ms){
		    min_counter_ms = counter.counter_ms;
		}

		// restart the game once the death timer expired
		if (counter.counter_ms < 0) {
			registry.deathTimers.remove(entity);
			screen.darken_screen_factor = 0;
            restart_game();
			return true;
		}
	}

	for (Entity entity : registry.archers.entities) {
		if (registry.arrowCooldowns.has(entity)) {
			auto& arrowCooldowns = registry.arrowCooldowns.get(entity);
			arrowCooldowns.timeSinceLastShot += elapsed_ms_since_last_update;
		}
	}

	// reduce window brightness if player dying
	screen.darken_screen_factor = 1 - min_counter_ms / 3000;
	
	//update parallax background based on player position
	Motion& m = registry.motions.get(player);
	float dx = m.position.x - renderer->camera_x;
	renderer->camera_x = -dx * cameraSpeed;
	float dy = m.position.y - renderer->camera_y;
	renderer->camera_y = dy * cameraSpeed;

	movementSystem.handle_inputs();
	handlePlayerAnimation(elapsed_ms_since_last_update);

	if (level.disappearing && !levelDisappeared) {
		levelDisappearTimer -= elapsed_ms_since_last_update;
		if (levelDisappearTimer <= 0) {
			for (Entity entity : registry.platforms.entities) {
				RenderRequest& r = registry.renderRequests.get(entity);
				r.used_texture = TEXTURE_ASSET_ID::EMPTY;
			}
			for (Entity entity : registry.walls.entities) {
				RenderRequest& r = registry.renderRequests.get(entity);
				r.used_texture = TEXTURE_ASSET_ID::EMPTY;
			}
			for (Entity entity : registry.deadlys.entities) {
				RenderRequest& r = registry.renderRequests.get(entity);
				r.used_texture = TEXTURE_ASSET_ID::EMPTY;
			}
			levelDisappeared = true;
		}
	}

	if (level.mouse_gesture) {

		if (!currDrawn) {
			handleDrawOnLines(currDrawing);
			currDrawn = true;
		}
		//printf("size: %d\n", registry.toDrawOns.size());
		bool allTouched = true;
		int tolerance = 200;
		if (registry.toDrawOns.size() == 0) {
			allTouched = false;
		}
		else {
			for (Entity e : registry.toDrawOns.entities) {
				Motion m = registry.motions.get(e);
				float x = m.position.x;
				float y = m.position.y;

				bool currTouched = false;
				for (Entity el : registry.drawnLines.entities) {
					DrawnLine d = registry.drawnLines.get(el);
					DrawnPoint p1 = registry.drawnPoints.get(d.p1);
					DrawnPoint p2 = registry.drawnPoints.get(d.p2);
					float x1 = p1.position.x;
					float y1 = p1.position.y;
					float x2 = p2.position.x;
					float y2 = p2.position.y;
					//printf("x: %f, y: %f\n", x, y);
					//printf("x1: %f, y1: %f\n", x1, y1);
					//printf("x2: %f, y2: %f\n", x2, y2);
					if (std::abs((y - y1) * (x2 - x1) - (y2 - y1) * (x - x1)) <= tolerance) {
						float left = x - m.scale.x;
						float right = x + m.scale.x;
						float top = y - m.scale.y;
						float bottom = y + m.scale.y;
						//printf("left: %f, right: %f, top: %f, bottom: %f\n", left, right, top, bottom);
						if (aiSystem.line_intersects_box(x1,y1,x2,y2,left,top,right,bottom)) {
							//printf("ever true?");
							registry.toDrawOns.remove(e);
							currTouched = true;
							break;
						}
					}
				}
				if (!currTouched) {
					allTouched = false;
					break;
				}
			}
		}

		if (allTouched) {
			printf("all touched\n");
			currDrawing++;
			currDrawn = false;
		}
		else {
			//printf("not all touched\n");
		}

		//printf("level: %d\n", currDrawing);
	}

	// clear rendering list for hint text
	renderer->hints.clear();

	for (Entity e : registry.hints.entities) {
		switchHintAnimation(e, elapsed_ms_since_last_update);
	}

	return true;
}

void WorldSystem::handlePlayerAnimation(float elapsed_ms_since_last_update) {
	static const float targetFrameTime = 1000.0f / 24.0f; // Target time for each frame (24 FPS)

	Motion& m = registry.motions.get(player);
	static float accumulatedTime = 0.0f;

	// Add the elapsed time since the last update to the accumulated time
	accumulatedTime += elapsed_ms_since_last_update;

	// Perform animation updates based on the target frame time
	while (accumulatedTime >= targetFrameTime) {
		// if moving and grounded
		if (movementSystem.leftOrRight() && m.grounded) {
			// Calculate next frame for texture change
			currentRunningTexture++;
			if (currentRunningTexture > (int)TEXTURE_ASSET_ID::RUN9) {
				currentRunningTexture = (int)TEXTURE_ASSET_ID::OLIVER;
			}
			registry.renderRequests.get(player).used_texture = static_cast<TEXTURE_ASSET_ID>(currentRunningTexture);
		}
		else if (!m.grounded) {
			// Player is not grounded (in air)
			registry.renderRequests.get(player).used_texture = TEXTURE_ASSET_ID::RUN7;
		}
		else if (m.grounded) {
			// Player is grounded
			registry.renderRequests.get(player).used_texture = TEXTURE_ASSET_ID::OLIVER;
		}

		// Subtract the target frame time from the accumulated time
		accumulatedTime -= targetFrameTime;
	}
}

void WorldSystem::displayTutorialImage() {
	// render tutorial images
	if (WorldSystem::level_idx == 0)
		tutorial = createTutorialMove(renderer);
	else if (WorldSystem::level_idx == 1) {
		registry.renderRequests.remove(tutorial);
		tutorial = createTutorialJump(renderer);
	}
	else if (WorldSystem::level_idx == 2) {
		registry.renderRequests.remove(tutorial);
		tutorial = createTutorialMainMenu(renderer);
	}
	else if (WorldSystem::level_idx == 3) {
		registry.renderRequests.remove(tutorial);
		//should be tutorial on checkpoints
	}
	else if (WorldSystem::level_idx == 4) {
		registry.renderRequests.remove(tutorial);
		tutorial = createTutorialDraw(renderer);
	}
	else if (registry.renderRequests.has(tutorial))
		registry.renderRequests.remove(tutorial);
}

Level WorldSystem::createLevel(int level_idx) {
	printf("creating level: %i\n", level_idx);
	Level level = this->levelManager.levels[level_idx];
	int platformHeight;
	for (InitWall w : level.walls) {
		platformHeight = abs(w.y - window_height_px) + w.ySize / 2 + 2;
		createPlatform(renderer, { w.x, window_height_px - platformHeight }, { w.xSize - 10, 10.f });
		createWall(renderer, { w.x, w.y }, { w.xSize, w.ySize });
	}
	for (InitSpike s : level.spikes) {
		createSpikes(renderer, { s.x, s.y }, { 40, 20 }, s.angle);
	}
	if (level.hasCheckpoint) {
		createCheckpoint(renderer, level.checkpoint);
	}
	createEndpoint(renderer, level.endPoint);
	for (InitHint hint : level.hints) {
		createHint(renderer, hint.npcPos, hint.text, hint.textPos);
	}
	if (level.hasChaseBoulder) {
		chaseBoulder = createChaseBoulder(renderer, level.chaseBoulder);
		bestPath = {};
		currentNode = 0;
	}
	for (vec2 archer : level.archers) {
		createArcher(renderer, archer, vec2(70.f));
	}
	for (InitPaintCan paintcan : level.paintcans) {
		createPaintCan(renderer, paintcan.pos, vec2(25, 50), paintcan.value, paintcan.fixed);
	}
	player = createOliver(renderer, level.playerSpawn);
	return level;
}

// Reset the world state to its initial state
void WorldSystem::restart_game() {
	// Debugging for memory/component leaks
	registry.list_all_components();
	printf("Restarting\n");

	// Reset the game speed
	current_speed = 1.f;

	movementSystem.reset();
	drawings.stop_drawing();

	// Remove all entities that we created
	while (registry.motions.entities.size() > 0)
	    registry.remove_all_components_of(registry.motions.entities.back());

	// Remove pencil
	while (registry.pencil.entities.size() > 0)
		registry.remove_all_components_of(registry.pencil.entities.back());

	// Debugging for memory/component leaks
	registry.list_all_components();
	
	//createBackground(renderer);

	// reset level
	//displayTutorialImage();
	//createLevel();
	WorldSystem::level = createLevel(level_idx);
	drawings.reset(level.inkLimit);
	
	// Create pencil
	pencil = createPencil(renderer, { window_width_px / 2, window_height_px / 2 }, { 50.f, 50.f });

	Motion& mParticle = registry.motions.get(pencil);
	ParticleEmitter& emitter = registry.particleEmitters.emplace(pencil);
	emitter.emission_point = { mParticle.position.x - 20, mParticle.position.y + 30 };
	emitter.particles_per_second = 1;
	emitter.initial_velocity = {0, 10.f};
	emitter.color = { 1.f, 0.f, 0.f, 1.f };
	emitter.lifespan = 1.0f;

	// Create test paint can

	// Center cursor to pencil location
	glfwSetCursorPos(window, window_width_px / 2 - 25.f, window_height_px / 2 + 25.f);

	levelDisappearTimer = level.disappearing_timer;
	levelDisappeared = false;

	currDrawing = 0;
	currDrawn = false;

	*mainMenu = false;
	renderer->renderMainMenuText = false;
}

void WorldSystem::handleLineCollision(const Entity& line, float elapsed_ms) {
	// Calculate two directions to apply force: perp. to line, and parallel to line
	//const DrawnLine& l = registry.drawnLines.get(line);
	//const Motion& lm = registry.motions.get(line);
	//float perp_slope = -1 / l.slope; // negative reciprocal gets orthogonal line
	//float step_seconds = elapsed_ms / 1000.f;
	//
	//vec2 parallel(1, l.slope);
	//parallel = normalize(parallel);

	//vec2 perp(1, perp_slope);
	//perp = normalize(perp);

	//if (abs(l.slope) < 0.001) { // effectively zero slope
	//	parallel = {1,0};
	//	perp = {0,1};
	//}

	//Motion &pm = registry.motions.get(player);
	//vec2 proj = dot(pm.velocity, perp) * perp;
	//pm.velocity -= proj;
	//pm.position = pm.last_position + pm.velocity  * step_seconds;

	// https://www.reddit.com/r/gamedev/comments/28m3xy/how_do_you_handle_slopes_in_a_2d_physics_engine/
	// // if above is true, grounded is true.
	// update position based on the movement:
	// if the above conditions were true, update position based on grounded
	const DrawnLine& l = registry.drawnLines.get(line);
	const Motion& lm = registry.motions.get(line);
	Motion& pm = registry.motions.get(player);
	// if player is above line, set player to grounded and not jumping (remember above is actually smaller y values)
	float line_y_pos = l.slope * (pm.position.x - lm.position.x) + lm.position.y;
	float pm_bot_pos = pm.position.y + pm.scale.y / 2;
	float slope_threshold = 2.f;
	if (pm_bot_pos <= line_y_pos) {
		// if slope of line isn't too steep, then update player position on y
		if (abs(l.slope) <= slope_threshold) {
			float mx = l.slope * (pm.position.x - pm.last_position.x);
			// if moving the player up a slope:
			if (mx < 0) {
				pm.position.y = mx + pm.last_position.y;
			}
		}
	}
	else {
		pm.position = pm.last_position;
		pm.velocity.y = 0;
	}
}

// handle all registered collisions
void WorldSystem::handle_collisions(float elapsed_ms) {
	// Loop over all collisions detected by the physics system
	auto& collisionsRegistry = registry.collisions;
	Motion& pMotion = registry.motions.get(player);
	for (uint i = 0; i < collisionsRegistry.components.size(); i++) {
		// The entity and its collider
		Entity entity = collisionsRegistry.entities[i];
		Entity entity_other = collisionsRegistry.components[i].other;

		// player collisions
		if (registry.players.has(entity)) {
			//Player& player = registry.players.get(entity);

			// Checking Player - Deadly collisions
			if (registry.deadlys.has(entity_other)) {
				// initiate death unless already dying
				if (!registry.deathTimers.has(entity)) {
					// Scream, reset timer, and make the chicken sink
					registry.deathTimers.emplace(entity);
					Mix_PlayChannel(-1, dead_sound, 0);
					pMotion.fixed = true;
					if (drawings.currently_drawing())
						drawings.stop_drawing();
				}
			}
			// Checking Player - Eatable collisions
			else if (registry.eatables.has(entity_other)) {
				if (!registry.deathTimers.has(entity)) {
					PaintCan p = registry.paintCans.get(entity_other);
					drawings.add_drawing_count(p.paintRefill);
					registry.remove_all_components_of(entity_other);
					Mix_PlayChannel(-1, ink_pickup_sound, 0);
				}
			}
			else if (registry.walls.has(entity_other)) {
				Motion& pMotion = registry.motions.get(entity);
				Motion& wMotion = registry.motions.get(entity_other);
				float leftMax = wMotion.position.x - wMotion.scale.x / 2 + 50;
				float rightMax = wMotion.position.x + wMotion.scale.x / 2 - 50;
				if (pMotion.position.x <= leftMax) {
					pMotion.position.x = leftMax - 70;
				}
				else {
					pMotion.position.x = rightMax + 70;
				}
			}

			// Checking Player - Checkpoint collisions
			else if (registry.checkpoints.has(entity_other)) {
				if (checkPointAudioPlayer == false) {
					Mix_PlayChannel(-1, checkpoint_sound, 0);
					save_checkpoint();
					checkPointAudioPlayer = true;
				}
			}

			else if (registry.levelEnds.has(entity_other)) {
				Mix_PlayChannel(-1, level_win_sound, 0);
				next_level();
			}

			else if (registry.drawnLines.has(entity_other) && isLineCollisionsOn()) {
				handleLineCollision(entity_other, elapsed_ms);
				// if the player hit the line that's being currently drawn, cause player death to occur
				if (entity_other == drawings.get_prev_line() && drawings.currently_drawing() && !registry.deathTimers.has(entity)) {
					registry.deathTimers.emplace(entity);
					Mix_PlayChannel(-1, dead_sound, 0);
					pMotion.fixed = true;
					if (drawings.currently_drawing())
						drawings.stop_drawing();
				}
			}

			// if colliding with hint npc, add it to the list of hint text to render
			else if (registry.hints.has(entity_other)) {
				renderer->hints.push_back(registry.hints.get(entity_other));
			}
		}

		if (registry.projectiles.has(entity) && !(registry.projectiles.has(entity_other) || registry.pencil.has(entity_other))) {
			registry.remove_all_components_of(entity);
		}
	}

	// Remove all collisions from this simulation step
	registry.collisions.clear();
}


void WorldSystem::next_level() {
	if (level_idx == config.max_level) {
		level_idx = 0;
		restart_game();
		renderer->endScreen = true;
	}
	else {
		level_idx++;
		restart_game();
	}
	checkPointAudioPlayer = false;
}

void WorldSystem::save_checkpoint() {
	// find checkpoint position to save the player at the checkpoint spot
	Motion m1;
	for (int i = 0; i < registry.renderRequests.size(); i++) {
		if (registry.renderRequests.components[i].used_texture == TEXTURE_ASSET_ID::CHECKPOINT) {
			m1 = registry.motions.get(registry.renderRequests.entities[i]);
			break;
		}
	}
	Motion m = registry.motions.get(player);
	json j;
	// Save position, but not velocity; no need to preserve momentum from time of save
	j["position"]["x"] = m1.position[0];
	j["position"]["y"] = m1.position[1];
	j["scale"]["x"] = m.scale[0];
	j["scale"]["y"] = m.scale[1];
	j["gravity"] = m.gravityScale;
	j["level"] = level_idx;

	std::ofstream o("../save.json");
	o << j.dump() << std::endl;
}

void WorldSystem::load_checkpoint() {
	std::ifstream i("../save.json");
	// Ensure file exists
	if (!i.good())
		return;
	
	json j = json::parse(i);

	int currentLevel = j["level"];
	if (currentLevel == level_idx) {
		// reset game to default then reposition player
		restart_game();
		Motion& m = registry.motions.get(player);
		m.position[0] = j["position"]["x"];
		m.position[1] = j["position"]["y"];
		m.scale[0] = j["scale"]["x"];
		m.scale[1] = j["scale"]["y"];
		m.gravityScale = j["gravity"];
	}

}

// Should the game be over ?
bool WorldSystem::is_over() const {
	return bool(glfwWindowShouldClose(window));
}

// On key callback
void WorldSystem::on_key(int key, int, int action, int mod) {
	//close on escape
	if (action == GLFW_RELEASE && key == GLFW_KEY_ESCAPE) {
		if (!*mainMenu) {
			*mainMenu = true;
			mainMenuEntity = createMainMenu(renderer, { window_width_px / 2, window_height_px / 2 }, { 800,800 });
			renderer->renderMainMenuText = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else {
			*mainMenu = false;
			renderer->renderMainMenuText = false;
			registry.remove_all_components_of(mainMenuEntity);
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
		}
		/*createLine({window_width_px / 2 - 120, window_height_px / 2 + 20}, {5, 20}, 0);
		createLine({ window_width_px / 2 + 130, window_height_px / 2 + 20 }, { 5, 20 }, 0);

		createLine({ window_width_px / 2, window_height_px / 2 - 145 }, { 5, 20 }, M_PI / 2);
		createLine({ window_width_px / 2, window_height_px / 2 - 80 }, { 5, 20 }, M_PI / 2);

		createLine({ window_width_px / 2, window_height_px / 2 }, { 5, 20 }, M_PI / 2);
		createLine({ window_width_px / 2, window_height_px / 2 + 63 }, { 5, 20 }, M_PI / 2);

		createLine({ window_width_px / 2, window_height_px / 2 + 145 }, { 5, 20 }, M_PI / 2);
		createLine({ window_width_px / 2, window_height_px / 2 + 207 }, { 5, 20 }, M_PI / 2);*/
		//glfwSetWindowShouldClose(window, 1);
	}

	// player movement
	if (!registry.deathTimers.has(player) && !RenderSystem::introductionScreen && !RenderSystem::endScreen && (key == GLFW_KEY_A || key == GLFW_KEY_D)) {
		RenderRequest& renderRequest = registry.renderRequests.get(player);
		if (action != GLFW_RELEASE) {
			movementSystem.press(key);
		}
		else if (action == GLFW_RELEASE) {
			movementSystem.release(key);
		}
	}

	// player jump
	if (!registry.deathTimers.has(player) && !RenderSystem::introductionScreen && !RenderSystem::endScreen && key == GLFW_KEY_SPACE) {
		if (action == GLFW_PRESS) {
			movementSystem.press(key);
		}
		else if (action == GLFW_RELEASE) {
			movementSystem.release(key);
		}
	}


	// Resetting game
	if (action == GLFW_RELEASE && key == GLFW_KEY_R && !RenderSystem::introductionScreen && !RenderSystem::endScreen) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);

        restart_game();
	}

	//skipping cutscene
	if (action == GLFW_RELEASE && key == GLFW_KEY_Z && (RenderSystem::introductionScreen == true || RenderSystem::endScreen == true)) {
		RenderSystem::introductionScreen = false;
		RenderSystem::endScreen = false;
		restart_game();
	}

	// Loading game
	if (action == GLFW_RELEASE && key == GLFW_KEY_L && !RenderSystem::introductionScreen && !RenderSystem::endScreen) {
		load_checkpoint();
	}

	// Debugging
	if (key == GLFW_KEY_I && action == GLFW_PRESS) {
		debugging.in_debug_mode = !debugging.in_debug_mode;
	}

	// Control the current speed with `<` `>`
	if (action == GLFW_RELEASE && (mod & GLFW_MOD_SHIFT) && key == GLFW_KEY_COMMA) {
		current_speed -= 0.1f;
		printf("Current speed = %f\n", current_speed);
	}
	if (action == GLFW_RELEASE && (mod & GLFW_MOD_SHIFT) && key == GLFW_KEY_PERIOD) {
		current_speed += 0.1f;
		printf("Current speed = %f\n", current_speed);
	}
	current_speed = fmax(0.f, current_speed);

	//next level
	if (action == GLFW_RELEASE && key == GLFW_KEY_EQUAL) {
		if (level_idx < config.max_level) {
			level_idx++;
			restart_game();
		}
	}
	if (action == GLFW_RELEASE && key == GLFW_KEY_MINUS) {
		if (level_idx > 0) {
			level_idx--;
			restart_game();
		}
	}
}

void WorldSystem::on_mouse_move(vec2 mouse_position) {
	if (*mainMenu) {
		restart = false;
		resume = false;
		exit = false;
		if (mouse_position.x > window_width_px / 2 - 120 && mouse_position.x < window_width_px / 2 + 130) {
			if (mouse_position.y > window_height_px / 2 - 145 && mouse_position.y < window_height_px / 2 - 80) {
				//printf("resume\n");
				resume = true;
			}
			else if (mouse_position.y > window_height_px / 2 && mouse_position.y < window_height_px / 2 + 63) {
				//printf("restart\n");
				restart = true;
			}
			else if (mouse_position.y > window_height_px / 2 + 145 && mouse_position.y < window_height_px / 2 + 207) {
				//printf("exit\n");
				exit = true;
			}
		}
	}
	else {
		if (mouse_position.x < 0 || mouse_position.x > window_width_px
			|| mouse_position.y < 0 || mouse_position.y > window_height_px)
			return;
		Motion& m = registry.motions.get(pencil);
		m.position.x = mouse_position.x + 25.f;
		m.position.y = mouse_position.y - 25.f;

		drawings.set_draw_pos(mouse_position);
	}
}

void WorldSystem::on_mouse_click(int button, int action, int mod) {
	if (*mainMenu) {
		if (resume) {
			*mainMenu = false;
			renderer->renderMainMenuText = false;
			registry.remove_all_components_of(mainMenuEntity);
		}
		else if (restart) {
			restart_game();
		}
		else if (exit) {
			glfwSetWindowShouldClose(window, 1);
		}
	}
	else if (RenderSystem::introductionScreen) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
			renderer->sceneIndex++;
			if (renderer->sceneIndex == 13) {
				renderer->introductionScreen = false;
				renderer->sceneIndex = 0;
			}
		}
	}
	else if (RenderSystem::endScreen) {
		if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
			renderer->sceneIndex++;
			if (renderer->sceneIndex == 13) {
				renderer->endScreen = false;
				renderer->sceneIndex = 0;
			}
		}
	}
	else {
		static const int DRAW_BUTTON = GLFW_MOUSE_BUTTON_LEFT;
		if (button == DRAW_BUTTON) {
		       if (action == GLFW_PRESS && !registry.deathTimers.has(player)) {
			       drawings.start_drawing();
		       }
		       else if (action == GLFW_RELEASE) {
			       drawings.stop_drawing();
		       }
		}

	}
}

