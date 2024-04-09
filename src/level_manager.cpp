#include "level_manager.hpp"

void LevelManager::loadLevels() {
	for (int i = 0; i < numLevels; i++) {
		structLevels.push_back(loadLevel(i));
	}
	return;
}

LevelStruct LevelManager::loadLevel(int levelNumber) {
	std::stringstream file_path;
	file_path << level_path() << "/" << levelNumber << ".json";
	printf("level file: %s\n", file_path.str().c_str());
	std::ifstream file(file_path.str());
	json levelData = json::parse(file);

	LevelStruct levelObject;

	for (json wall : levelData["walls"]) {
		levelObject.walls.push_back(parseWall(wall));
	}

	for (json stair : levelData["stairs"]) {
		parseStair(levelObject, stair);
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

void LevelManager::parseStair(LevelStruct level, json stairJson) {
	int numStairs = stairJson["quantity"];
	int startX = stairJson["x"];
	int startY = stairJson["y"];
	int stairWidth = stairJson["width"];
	int stairHeight = stairJson["height"];
	int stairGap = stairJson["gap"];
	for (int i = 0; i < numStairs; ++i) {
		InitWall stair;
		stair.x = startX + i * (stairWidth + stairGap);
		stair.y = startY - i * (stairHeight + stairGap);
		stair.xSize = stairWidth;
		stair.ySize = stairHeight;
		level.walls.push_back(stair);
	}
}

void LevelManager::initLevel() {
	//level 1
	InitWall w1_2 = { window_width_px - 1000, window_height_px - 60, window_width_px - 600, 400 };
	Level Level1;
	Level1.walls.push_back(w1_2);
	Level1.endPoint.first = window_width_px - 400;
	Level1.endPoint.second = window_height_px - 310;
	Level1.playerPos.first = window_width_px / 2 - 400;
	Level1.playerPos.second = window_height_px - 350;
	Level1.hintPos.first = window_width_px / 2 - 350;
	Level1.hintPos.second = window_height_px - 298;
	Level1.hint = "Hello, I am the hint guy, I will give you a hint on certain levels! For now, just follow the tutorial and reach the trophy";
	Level1.hintTextPos.first = window_width_px / 2 - 450;
	Level1.hintTextPos.second = window_height_px - 560;

	levels.push_back(Level1);

	//level 2
	InitWall w2 = { 900, window_height_px - 70, 1000, 600 };
	InitWall w2_1 = { window_width_px - window_width_px + 40, window_height_px - 100, 400, 400 };
	InitWall w2_2 = { window_width_px - 200, window_height_px - 230, 500, 455 };
	Level Level2;
	Level2.walls.push_back(w2);
	Level2.walls.push_back(w2_1);
	Level2.walls.push_back(w2_2);
	Level2.endPoint.first = window_width_px - 200;
	Level2.endPoint.second = window_height_px - 505;
	Level2.playerPos.first = 100;
	Level2.playerPos.second = 460;
	levels.push_back(Level2);

	//level 3
	InitWall w3 = { 900, window_height_px - 70, 1000, 600 };
	InitWall w3_1 = { window_width_px - window_width_px + 40, window_height_px - 100, 400, 400 };
	InitWall w3_2 = { window_width_px - 200, window_height_px - 60, 500, 400 };
	Level Level3;

	Level3.walls.push_back(w3);
	Level3.walls.push_back(w3_1);
	Level3.walls.push_back(w3_2);

	Level3.endPoint.first = window_width_px - 200;
	Level3.endPoint.second = window_height_px - 305;

	Level3.playerPos.first = 100;
	Level3.playerPos.second = 460;

	Level3.hintPos.first = window_width_px / 2 - 350;
	Level3.hintPos.second = window_height_px - 402;
	Level3.hint = "Hit escape for the main menu";
	Level3.hintTextPos.first = window_width_px / 2 - 350;
	Level3.hintTextPos.second = window_height_px - 475;

	levels.push_back(Level3);

	//level 4
	InitWall w4_1 = { window_width_px / 2, window_height_px, window_width_px, 600 };
	InitWall w4_2 = { window_width_px / 2, 90, window_width_px, window_height_px / 2 + 300 };
	Level Level4;
	Level4.walls.push_back(w4_1);
	Level4.walls.push_back(w4_2);

	createSpikes(Level4, 100, 503, 46, 40, M_PI);

	createSpikes(Level4, 160, 685, 5, 120, 0);

	createSpikes(Level4, 1200, 685, 5, 120, 0);

	Level4.checkpoint.first = window_width_px / 2;
	Level4.checkpoint.second = 643;

	Level4.endPoint.first = window_width_px - 200;
	Level4.endPoint.second = 643;

	Level4.playerPos.first = 10;
	Level4.playerPos.second = 560;

	Level4.hintPos.first = window_width_px / 2 - 80;
	Level4.hintPos.second = 663;
	Level4.hint = "Too hard? Hit the flag and press L to restart there";
	Level4.hintTextPos.first = window_width_px / 2 - 80;
	Level4.hintTextPos.second = window_height_px - 545;

	levels.push_back(Level4);

	//level 5
	InitWall w5 = { 900, window_height_px - 90, 1000, 600 };
	InitWall w5_1 = { window_width_px - window_width_px, window_height_px - 100, 400, 400 };
	InitWall w5_2 = { window_width_px - 200, window_height_px - 60, 500, 400 };
	Level Level5;

	Level5.walls.push_back(w5);
	Level5.walls.push_back(w5_1);
	Level5.walls.push_back(w5_2);

	Level5.checkpoint.first = window_width_px - 300;
	Level5.checkpoint.second = window_height_px - 305;

	Level5.endPoint.first = window_width_px - 200;
	Level5.endPoint.second = window_height_px - 305;

	Level5.playerPos.first = window_width_px / 2 - 300;
	Level5.playerPos.second = 460;

	Level5.hintPos.first = window_width_px / 2 - 80;
	Level5.hintPos.second = 575;
	Level5.hint = "Drawings can block line of sight of boulders, destroy projectiles, block paths of paintcans and archers, and a lot more!";
	Level5.hintTextPos.first = window_width_px / 2 - 650;
	Level5.hintTextPos.second = window_height_px - 450;
	levels.push_back(Level5);


	//level 6
	Level Level6;

	int startY = window_height_px - 200;
	int stairWidth = 100;
	int stairHeight = 20;
	int numStairs = 10;
	int stairGap = 50;
	int startX = (window_width_px - numStairs * (stairWidth + stairGap)) / 2;


	createStairs(numStairs, Level6, startX, startY, stairWidth, stairGap, stairHeight);

	Level6.checkpoint.first = startX + (numStairs / 2) * (stairWidth + stairGap) + 10;
	Level6.checkpoint.second = startY - (numStairs / 2) * (stairHeight + stairGap) - (stairHeight / 2) - 55;

	Level6.playerPos.first = startX;
	Level6.playerPos.second = startY - (numStairs - 1) * (stairHeight + stairGap) - stairHeight;

	Level6.endPoint.first = startX + (numStairs - 1) * (stairWidth + stairGap) + 10;
	Level6.endPoint.second = startY - (numStairs - 1) * (stairHeight + stairGap) - 65;

	Level6.hintPos.first = startX + 10;
	Level6.hintPos.second = 750;
	Level6.hint = "Congratz! You finished the tutorial... Now the real fun begins.";
	Level6.hintTextPos.first = startX + 10;
	Level6.hintTextPos.second = window_height_px - 630;


	levels.push_back(Level6);


	Level Level7;
	InitWall w_7 = { window_width_px - 500, window_height_px - 200, 1000, 600 };
	InitWall w7_2 = { window_width_px - window_width_px, window_height_px - 100, 400, 400 };

	Level7.walls.push_back(w_7);
	Level7.walls.push_back(w7_2);

	stairWidth = 150;
	stairHeight = 20;
	numStairs = 3;
	stairGap = 80;

	startX = w7_2.x + w7_2.xSize;
	startY = w7_2.y - 2 * stairHeight - 200;

	createStairs(numStairs, Level7, startX, startY, stairWidth, stairGap, stairHeight);

	Level7.checkpoint.first = Level7.walls.back().x + Level7.walls.back().xSize / 2 - 10;
	Level7.checkpoint.second = Level7.walls.back().y - Level7.walls.back().ySize / 2 - 55;

	Level7.playerPos.first = 100;
	Level7.playerPos.second = window_height_px - 400;

	Level7.endPoint.first = w_7.x + w_7.xSize / 2 - 100;
	Level7.endPoint.second = w_7.y - w_7.ySize / 2 - 55;

	levels.push_back(Level7);

	//level 8
	Level Level8;
	InitWall w_8 = { 0, window_height_px - 200, 600, 600 };

	Level8.walls.push_back(w_8);

	InitWall wall8_1 = { 400, window_height_px - 400, 150, 20 };
	InitWall wall8_2 = { 700, window_height_px - 400, 150, 20 };
	InitWall wall8_3 = { 1040, window_height_px - 400, 150, 20 };
	InitWall wall8_4 = { 1300, window_height_px - 500, 150, 20 };
	InitWall wall8_5 = { 1500, window_height_px - 500, 150, 20 };
	InitWall wall8_6 = { 1840, window_height_px - 500, 200, 20 };

	Level8.walls.push_back(wall8_1);
	Level8.walls.push_back(wall8_2);
	Level8.walls.push_back(wall8_3);
	Level8.walls.push_back(wall8_4);
	Level8.walls.push_back(wall8_5);
	Level8.walls.push_back(wall8_6);

	Spike spike1 = { 350, window_height_px - 425, 0 };
	Spike spike2 = { 387, window_height_px - 425, 0 };
	Spike spike3 = { 1455, window_height_px - 523, 0 };
	Spike spike4 = { 1495, window_height_px - 523, 0 };
	Spike spike5 = { 1825, window_height_px - 523, 0 };
	Spike spike6 = { 1860, window_height_px - 523, 0 };

	Level8.spikes.push_back(spike1);
	Level8.spikes.push_back(spike2);
	Level8.spikes.push_back(spike3);
	Level8.spikes.push_back(spike4);
	Level8.spikes.push_back(spike5);
	Level8.spikes.push_back(spike6);

	Level8.checkpoint.first = -10;
	Level8.checkpoint.second = -10;

	Level8.playerPos.first = 10;
	Level8.playerPos.second = window_height_px - 800;

	Level8.endPoint.first = wall8_6.x + wall8_6.xSize / 2 - 20;
	Level8.endPoint.second = wall8_6.y - wall8_6.ySize / 2 - 55;

	Level8.hintPos.first = 150;
	Level8.hintPos.second = window_height_px - 535;
	Level8.hint = "Hint: the levels will disappear.. There must be some way to help you memorize them";

	Level8.hintTextPos.first = 150;
	Level8.hintTextPos.second = window_height_px - 350;

	levels.push_back(Level8);

	//level 9
	Level level9;
	InitWall w_9 = { 200, window_height_px - 100, 400, 600 };

	level9.walls.push_back(w_9);

	InitWall w_9_1 = { window_width_px - 200, window_height_px - 100, 400, 600 };

	level9.walls.push_back(w_9_1);

	level9.playerPos.first = 200;
	level9.playerPos.second = window_height_px - 445;

	level9.endPoint.first = w_9_1.x + w_9_1.xSize / 2 - 200;
	level9.endPoint.second = w_9_1.y - w_9_1.ySize / 2 - 55;

	level9.checkpoint.first = NULL;
	level9.checkpoint.second = NULL;

	level9.hintPos.first = 150;
	level9.hintPos.second = window_height_px - 435;
	level9.hint = "Hint: follow the red outlines and there might be a surprise....";

	level9.hintTextPos.first = 150;
	level9.hintTextPos.second = window_height_px - 435;

	levels.push_back(level9);
}