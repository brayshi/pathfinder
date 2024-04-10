#include "level_manager.hpp"

void LevelManager::loadLevels() {
	for (int i = 0; i <= config.max_level; i++) {
		levels.push_back(loadLevel(i));
	}
	//levels.push_back(loadLevel(9));
	return;
}

Level LevelManager::loadLevel(int levelNumber) {
	std::stringstream file_path;
	file_path << level_path() << "/" << levelNumber << ".json";
	printf("level file: %s\n", file_path.str().c_str());
	std::ifstream file(file_path.str());
	json levelData = json::parse(file);

	Level levelObject;

	for (json wall : levelData["walls"]) {
		levelObject.walls.push_back(parseWall(wall));
	}

	// automatically ignores null arrays
	for (json stair : levelData["stairs"]) {
		parseStair(levelObject, stair);
	}

	// automatically ignores null arrays
	for (json spike : levelData["spikes"]) {
		parseSpike(levelObject, spike);
	}

	if (!levelData["checkpoint"].is_null()) {
		levelObject.hasCheckpoint = true;
		levelObject.checkpoint.x = levelData["checkpoint"][0];
		levelObject.checkpoint.y = levelData["checkpoint"][1];
	}

	levelObject.endPoint.x = levelData["level_end"][0];
	levelObject.endPoint.y = levelData["level_end"][1];

	for (json hint : levelData["hints"]) {
		levelObject.hints.push_back(parseHint(hint));
	}

	levelObject.gravity = levelData["gravity"].is_null() ? config.gravity : levelData["gravity"];
	levelObject.friction = levelData["friction"].is_null() ? config.friction : levelData["friction"];
	levelObject.inkLimit = levelData["ink_limit"].is_null() ? config.ink_limit : levelData["ink_limit"];
	levelObject.lineCollisionOn = levelData["line_collision_on"].is_null() ? config.line_collision_on : levelData["line_collision_on"];
	levelObject.disappearing = levelData["disappearing"].is_null() ? levelObject.disappearing : levelData["disappearing"];
	levelObject.disappearing_timer = levelData["disappearing_timer"].is_null() ? config.disappearing_timer : levelData["disappearing_timer"];
	levelObject.mouse_gesture = levelData["mouse_gesture"].is_null() ? levelObject.mouse_gesture : levelData["mouse_gesture"];

	levelObject.playerSpawn.x = levelData["player_spawn"][0];
	levelObject.playerSpawn.y = levelData["player_spawn"][1];
	
	auto spawnersJson = levelData["spawners"];
	if (!spawnersJson.is_null()) {
		if (!spawnersJson["boulder_spawners"].is_null()) {
			for (json boulderSpawner : spawnersJson["boulder_spawners"]) {
				levelObject.boulderSpawners.push_back(parseBoulderSpawner(boulderSpawner));
			}
		}
		if (!spawnersJson["spike_projectile_spawners"].is_null()) {
			for (json spikeProjectileSpawner : spawnersJson["spike_projectile_spawners"]) {
				levelObject.spikeProjectileSpawners.push_back(parseSpikeProjectileSpawner(spikeProjectileSpawner));
			}
		}
	}

	auto enemiesJson = levelData["enemies"];
	if (!enemiesJson.is_null()) {
		if (!enemiesJson["chase_boulder"].is_null()) {
			levelObject.hasChaseBoulder = true;
			levelObject.chaseBoulder.x = enemiesJson["chase_boulder"][0];
			levelObject.chaseBoulder.y = enemiesJson["chase_boulder"][1];
		}
		if (!enemiesJson["archers"].is_null()) {
			for (json archer : enemiesJson["archers"]) {
				levelObject.archers.push_back(glm::vec2(archer["x"], archer["y"]));
			}
		}
	}
	auto paintcans = levelData["paintcans"];
	if (!paintcans.is_null()) {
		for (json paintcan : paintcans) {
			levelObject.paintcans.push_back(parsePaintCan(paintcan));
		}
	}

	return levelObject;
}

InitWall LevelManager::parseWall(json wallJson) {
	InitWall wall;
	wall.x = wallJson["x"];
	wall.y = wallJson["y"];
	wall.xSize = wallJson["width"];
	wall.ySize = wallJson["height"];
	return wall;
}

void LevelManager::parseStair(Level& level, json stairJson) {
	int numStairs = stairJson["quantity"];
	int startX = stairJson["x"];
	int startY = stairJson["y"];
	int stairGap = stairJson["gap"];
	int stairWidth = stairJson["width"];
	int stairHeight = stairJson["height"];
	for (int i = 0; i < numStairs; ++i) {
		InitWall stair;
		stair.x = startX + i * (stairWidth + stairGap);
		stair.y = startY - i * (stairHeight + stairGap);
		stair.xSize = stairWidth;
		stair.ySize = stairHeight;
		level.walls.push_back(stair);
	}
}

void LevelManager::parseSpike(Level& level, json spikeJson) {
	vec2 init_pos;
	init_pos.x = spikeJson["x"];
	init_pos.y = spikeJson["y"];
	int numSpikes = spikeJson["quantity"];
	int gap = spikeJson["gap"];
	int angle_deg = spikeJson["angle"];
	float angle = angle_deg * (M_PI / 180);

	vec2 gap_vec = vec2(gap, 0);
	gap_vec = rotate_vector(gap_vec, angle);
	for (int i = 0; i < numSpikes; ++i) {
		InitSpike spike;
		spike.pos = init_pos + static_cast<float>(i) * gap_vec;
		spike.angle = angle;
		level.spikes.push_back(spike);
	}
}

InitHint LevelManager::parseHint(json hintJson) {
	InitHint hint;
	hint.npcPos.x = hintJson["npc"][0];
	hint.npcPos.y = hintJson["npc"][1];
	hint.text = hintJson["text"];
	hint.textPos.x = hintJson["text_pos"][0];
	hint.textPos.y = hintJson["text_pos"][1];
	return hint;
}

InitPaintCan LevelManager::parsePaintCan(json paintcanJson) {
	InitPaintCan paintcan;
	paintcan.pos.x = paintcanJson["x"];
	paintcan.pos.y = paintcanJson["y"];
	paintcan.value = paintcanJson["value"].is_null() ? config.paintcan_value : paintcanJson["value"];
	paintcan.fixed = paintcanJson["fixed"].is_null() ? paintcan.fixed : paintcanJson["fixed"];
	return paintcan;
}

InitBoulderSpawner LevelManager::parseBoulderSpawner(json boulderSpawnerJson) {
	InitBoulderSpawner boulderSpawner;
	boulderSpawner.pos.x = boulderSpawnerJson["x"];
	boulderSpawner.pos.y = boulderSpawnerJson["y"];
	boulderSpawner.random_x = boulderSpawnerJson["random_x"];
	boulderSpawner.delay = boulderSpawnerJson["delay"];
	return boulderSpawner;
}

InitSpikeProjectileSpawner LevelManager::parseSpikeProjectileSpawner(json spikeProjectileSpawnerJson) {
	InitSpikeProjectileSpawner spikeProjectileSpawner;
	spikeProjectileSpawner.pos.x = spikeProjectileSpawnerJson["x"];
	spikeProjectileSpawner.pos.y = spikeProjectileSpawnerJson["y"];
	spikeProjectileSpawner.size.x = spikeProjectileSpawnerJson["width"];
	spikeProjectileSpawner.size.y = spikeProjectileSpawnerJson["height"];
	spikeProjectileSpawner.vel.x = spikeProjectileSpawnerJson["x_vel"];
	spikeProjectileSpawner.vel.y = spikeProjectileSpawnerJson["y_vel"];
	int angle_deg = spikeProjectileSpawnerJson["angle"];
	spikeProjectileSpawner.angle = angle_deg * (M_PI / 180);
	spikeProjectileSpawner.delay = spikeProjectileSpawnerJson["delay"];
	return spikeProjectileSpawner;
}