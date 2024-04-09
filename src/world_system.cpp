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
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

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

void WorldSystem::init(RenderSystem* renderer_arg) {
	this->renderer = renderer_arg;
	// Playing background music indefinitely
	Mix_PlayMusic(background_music, -1);
	fprintf(stderr, "Loaded music\n");

	//init levels
	WorldSystem::level = config.starting_level;
	LevelManager lm;
	lm.initLevel();
	lm.printLevelsInfo();
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
	if ((level == 1) && registry.deadlys.components.size() <= MAX_BOULDERS && next_boulder_spawn < 0.f) {
		// Reset timer
		next_boulder_spawn = (BOULDER_DELAY_MS / 2) + uniform_dist(rng) * (BOULDER_DELAY_MS / 2);
		createBoulder(renderer, vec2(50.f + uniform_dist(rng) * (window_width_px - 100.f), -100.f));
	}

	if(!registry.deathTimers.has(player) && level == 2)
	{
		FrameCount += elapsed_ms_since_last_update;
		if (FrameCount >= FrameInterval) {
			aiSystem.updateGrid(levelManager.levels[level].walls);
			//aiSystem.printGrid();
			Motion& eMotion = registry.motions.get(advancedBoulder);
			Motion& pMotion = registry.motions.get(player);
			bestPath = aiSystem.bestPath(eMotion, pMotion);
			currentNode = 0;
			FrameCount = 0;
		}

		Motion& eMotion = registry.motions.get(advancedBoulder);

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
	renderer->camera_x = dx * cameraSpeed;
	float dy = m.position.y - renderer->camera_y;
	renderer->camera_y = dy * cameraSpeed;

	movementSystem.handle_inputs();
	handlePlayerAnimation(elapsed_ms_since_last_update);

	if (!level4Disappeared && level == 3) {
		level4DisappearTimer -= elapsed_ms_since_last_update;
		if (level4DisappearTimer <= 0) {
			Level& level4 = this->levelManager.levels[WorldSystem::level];
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
			level4Disappeared = true;
		}
	}

	return true;
}

void WorldSystem::handlePlayerAnimation(float elapsed_ms_since_last_update) {
	Motion& m = registry.motions.get(player);
	elapsedMsTotal += elapsed_ms_since_last_update;
	// if moving and grounded
	if (movementSystem.leftOrRight() && m.grounded) {
		// if enough time has elapsed, calculate next frame that we want to change the texture
		float minMsChange = 12.f;
		if (elapsedMsTotal > minMsChange) {
			currentRunningTexture += static_cast<int>(elapsedMsTotal / minMsChange);
			elapsedMsTotal = 0;
			if (currentRunningTexture > (int)TEXTURE_ASSET_ID::RUN6) {
				currentRunningTexture = (int)TEXTURE_ASSET_ID::OLIVER;
			}
			registry.renderRequests.get(player).used_texture = static_cast<TEXTURE_ASSET_ID>(currentRunningTexture);
		}
	}
	else if (!m.grounded) {
		registry.renderRequests.get(player).used_texture = TEXTURE_ASSET_ID::RUN4;
	}
	else if (m.grounded) {
		registry.renderRequests.get(player).used_texture = TEXTURE_ASSET_ID::OLIVER;
	}
}

void WorldSystem::createLevel() {
	if (WorldSystem::level == 0) 
		tutorial = createTutorial(renderer);
	else if (registry.renderRequests.has(tutorial))
		registry.renderRequests.remove(tutorial);

	Level currentLevel = this->levelManager.levels[WorldSystem::level];
	for (int i = 0; i < currentLevel.walls.size(); ++i) {
		initWall w = currentLevel.walls[i];
		createWall(renderer, {w.x, w.y}, {w.xSize, w.ySize});
		int platformHeight = abs(w.y - window_height_px) + w.ySize / 2 + 2;
		createPlatform(renderer, {w.x, window_height_px - platformHeight}, {w.xSize - 10, 10.f});
	}
	for (int i = 0; i < currentLevel.spikes.size(); ++i) {
		spike s = currentLevel.spikes[i];
		createSpikes(renderer, { s.x, s.y }, { 40, 20});
	}
	createCheckpoint(renderer, { currentLevel.checkpoint.first, currentLevel.checkpoint.second });
	createEndpoint(renderer, { currentLevel.endPoint.first, currentLevel.endPoint.second });
	player = createOliver(renderer, { currentLevel.playerPos.first, currentLevel.playerPos.second });
	registry.colors.insert(player, { 1, 1, 1 });	
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
	drawings.reset();

	// Remove all entities that we created
	while (registry.motions.entities.size() > 0)
	    registry.remove_all_components_of(registry.motions.entities.back());

	// Remove pencil
	while (registry.pencil.entities.size() > 0)
		registry.remove_all_components_of(registry.pencil.entities.back());

	// Debugging for memory/component leaks
	registry.list_all_components();
	
	//createBackground(renderer);

	//platform
	createLevel();
	
	// Create pencil
	pencil = createPencil(renderer, { window_width_px / 2, window_height_px / 2 }, { 50.f, 50.f });

	Motion& mParticle = registry.motions.get(pencil);
	ParticleEmitter& emitter = registry.particleEmitters.emplace(pencil);
	emitter.emission_point = { mParticle.position.x - 20, mParticle.position.y + 30 };
	emitter.particles_per_second = 1;
	emitter.initial_velocity = {0, 10.f};
	emitter.color = { 1.f, 0.f, 0.f, 1.f };
	emitter.lifespan = 0.4f;

	// Create test paint can

	// Center cursor to pencil location
	glfwSetCursorPos(window, window_width_px / 2 - 25.f, window_height_px / 2 + 25.f);

	if (level == 2) {
		advancedBoulder = createChaseBoulder(renderer, { window_width_px / 2, 100 });
		bestPath = {};
		currentNode = 0;
		createPaintCan(renderer, { window_width_px - 300, window_height_px / 2 }, { 25.f, 50.f });
		createArcher(renderer, { window_width_px - 600, window_height_px / 2 - 25}, { 70.f, 70.f });
	}

	level4DisappearTimer = 4000;
	level4Disappeared = false;

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
					// chew, count points, and set the LightUp timer
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

			else if (registry.drawnLines.has(entity_other)) {
				handleLineCollision(entity_other, elapsed_ms);
			}
		}

		if (registry.projectiles.has(entity)) {
			registry.remove_all_components_of(entity);
		}
	}

	// Remove all collisions from this simulation step
	registry.collisions.clear();
}


void WorldSystem::next_level() {
	if (level == maxLevel) {
		level = 0;
		restart_game();
		renderer->endScreen = true;
	}
	else {
		level++;
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
	j["level"] = level;

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
	if (currentLevel == level) {
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
		glfwSetWindowShouldClose(window, 1);
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
}

void WorldSystem::on_mouse_move(vec2 mouse_position) {
	if (mouse_position.x < 0 || mouse_position.x > window_width_px
	 || mouse_position.y < 0 || mouse_position.y > window_height_px)
		return;
	Motion& m = registry.motions.get(pencil);
	m.position.x = mouse_position.x + 25.f;
	m.position.y = mouse_position.y - 25.f;

	drawings.set_draw_pos(mouse_position);
}

void WorldSystem::on_mouse_click(int button, int action, int mod) {
	if (RenderSystem::introductionScreen) {
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

