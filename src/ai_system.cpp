// internal
#include "ai_system.hpp"
#include "vector"
#include <level_manager.hpp>
#include <iostream>

const int gridSize = 30;

const int gridWidth = window_width_px / gridSize;

const int gridHeight = window_height_px / gridSize;

int grid[gridHeight][gridWidth];

void AISystem::init() {
    for (int i = 0; i < gridHeight; i++) {
        for (int j = 0; j < gridWidth; j++) {
            grid[i][j] = 0;
        }
    };
}

void AISystem::updateGrid(std::vector<initWall> walls) {
    init();
    for (initWall w : walls) {
        int left = (w.x - w.xSize / 2) / gridSize;
        int top = (w.y - w.ySize / 2) / gridSize;
        int right = (w.x + w.xSize / 2) / gridSize;
        int bottom = (w.y + w.ySize / 2) / gridSize;
        if (bottom >= gridHeight) {
            bottom = gridHeight - 1;
        }
        if (left <= 0) {
            left = 0;
        }
        if (right >= gridWidth) {
            right = gridWidth - 1;
        }
        for (left; left <= right; left++) {
            for (top; top <= bottom; top++) {
                grid[top][left] = 1;
            }
             top = (w.y - w.ySize / 2) / gridSize;
        }
    }
}

void AISystem::printGrid() {
    printf("Chunk:\n");

    for (int r = 0; r < gridHeight; r++) {
        for (int c = 0; c < gridWidth; c++) {
            printf(" %x ", grid[r][c]);
        }
        printf("\n");
    }
    printf("--------\n");
}

struct CompareNodes {
    bool operator()(const Node* a, const Node* b) {
        return a->f() > b->f();
    }
};

int manhattanDistance(int x1, int y1, int x2, int y2) {
    return std::abs(x2 - x1) + std::abs(y2 - y1);
}

std::vector<std::pair<int, int>> AISystem::bestPath(Motion& eMotion, Motion& pMotion) {
    int startX = ceil(eMotion.position.x / gridSize) - 1;
    int startY = ceil(eMotion.position.y / gridSize) - 1;
    int targetX = ceil(pMotion.position.x / gridSize) - 1;
    int targetY = ceil(pMotion.position.y / gridSize) - 1;
    const int dx[4] = { 1, 0, -1, 0 };
    const int dy[4] = { 0, 1, 0, -1 };

    printf("targetX: %d ", targetX);
    printf("targetY: %d ", targetY);
    printf("gridHeight: %d ", gridHeight);

    if (grid[targetY][targetX] == 1 || targetY >= gridHeight) {
        return {};
    }

    /*printf("still calculating?");*/

    std::vector<std::pair<int, int>> path;
    int rows = gridHeight;
    int cols = gridWidth;

    std::vector<std::vector<bool>> closedList(rows, std::vector<bool>(cols, false));
    std::priority_queue<Node*, std::vector<Node*>, CompareNodes> openList;

    Node* start = new Node(startX, startY, 0, manhattanDistance(startX, startY, targetX, targetY), nullptr);
    openList.push(start);

    while (!openList.empty()) {
        Node* current = openList.top();
        openList.pop();
        if (current->x == targetX && current->y == targetY) {
            while (current != nullptr) {
                path.push_back({ current->x, current->y });
                current = current->parent;
            }
            std::reverse(path.begin(), path.end());
            break;
        }

        closedList[current->y][current->x] = true;

        for (int i = 0; i < 4; ++i) {
            int nextX = current->x + dx[i];
            int nextY = current->y + dy[i];
            if (nextX >= 0 && nextX < cols && nextY >= 0 && nextY < rows && grid[nextY][nextX] == 0 && !closedList[nextY][nextX]) {
                int g = current->g + 1;
                int h = manhattanDistance(nextX, nextY, targetX, targetY);
                Node* next = new Node(nextX, nextY, g, h, current);
                openList.push(next);
            }
        }
    }
    return path;
}



void AISystem::step(float elapsed_ms) {
    elapsed_ms_since_last_update += elapsed_ms;

    const float update_frequency = 1.0f / 60.0f * 1000.0f; // Convert 60 FPS to milliseconds

    if (elapsed_ms_since_last_update < update_frequency) {
        return;
    }

    elapsed_ms_since_last_update = 0;
    auto& motions_registry = registry.motions;
    vec2 player_position;
    bool player_found = false;
    Entity player;

    for (uint i = 0; i < motions_registry.size(); i++) {
        Motion& motion = motions_registry.components[i];
        Entity entity = motions_registry.entities[i];

        if (registry.players.has(entity)) {
            player_position = motion.position;
            player_found = true;
            player = entity;
            break;
        }
    }

    if (player_found) {
        // Boulder AI
        Motion& pmotion = registry.motions.get(player);
        vec2 pPosition = pmotion.position;
        vec2 pScale = pmotion.scale;
        // Using lerp to move towards player
        for (int i = (int)motions_registry.components.size() - 1; i >= 0; --i) {
            Motion& motion = motions_registry.components[i];
            Entity entity = motions_registry.entities[i];
            if (registry.boulders.has(entity)) {
                vec2 bPosition = motion.position;
                vec2 toPlayer = normalize(pPosition - bPosition);
                if (hasLineOfSight(vec2(bPosition.x, bPosition.y + motion.scale.y/2), pPosition)) {
                    vec2 targetVelocity = toPlayer * 300.f;
                    motion.position.x = lerp<float>(bPosition.x, pPosition.x, 0.003f);
                    motion.velocity.x = lerp<float>(motion.velocity.x, targetVelocity.x, 0.5f);
                    motion.acceleration.y = motion.acceleration.y * 10;
                }
            }
        }
        // PaintCan AI
        updatePaintCanMovement(player_position);
    }
}

bool AISystem::hasLineOfSight(const vec2& start, const vec2& end) {
    auto& motions_registry = registry.motions;

    float minX = std::min(start.x, end.x);
    float maxX = std::max(start.x, end.x);
    float minY = std::min(start.y, end.y);
    float maxY = std::max(start.y, end.y);

    for (uint i = 0; i < motions_registry.size(); i++) {
        Motion& motion = motions_registry.components[i];
        Entity entity = motions_registry.entities[i];

        if (!registry.platforms.has(entity)) continue;

        vec2 platformPos = motion.position;
        vec2 platformSize = motion.scale;

        // Calculate platform's AABB
        float platformLeft = platformPos.x - platformSize.x / 2.0f;
        float platformRight = platformPos.x + platformSize.x / 2.0f;
        float platformTop = platformPos.y + platformSize.y / 2.0f;
        float platformBottom = platformPos.y - platformSize.y / 2.0f;

        // Check if the platform intersects the line range in x and y 
        bool intersectsX = (platformLeft <= maxX && platformRight >= minX);
        bool intersectsY = (platformBottom <= maxY && platformTop >= minY);

        if (intersectsX && intersectsY) {
            return false;
        }
    }
    return true;
}

void AISystem::updatePaintCanMovement(const vec2& player_position) {
    auto& paintCanRegistry = registry.paintCans;
    auto& platformContainer = registry.platforms;
    auto& motionRegistry = registry.motions;
    float safeDistance = 150.f;

    for (auto& paintCanEntity : paintCanRegistry.entities) {
        Motion& paintCanMotion = motionRegistry.get(paintCanEntity);
        if (!paintCanMotion.grounded) continue;

        bool playerIsClose = glm::distance(paintCanMotion.position, player_position) < safeDistance;

        // Move away from player if too close, ignoring platform edges
        if (playerIsClose) {
            if (paintCanMotion.position.x < player_position.x) {
                paintCanMotion.velocity.x = -200.f; 
            }
            else {
                paintCanMotion.velocity.x = 200.f; 
            }
        }
        else {
            for (auto& platformEntity : platformContainer.entities) {
                Motion& platformMotion = motionRegistry.get(platformEntity);
                if (rectangleCollides(paintCanMotion, platformMotion)) {
                    double platformLeftEdge = platformMotion.position.x - (platformMotion.scale.x / 2.0);
                    double platformRightEdge = platformMotion.position.x + (platformMotion.scale.x / 2.0);

                    // Check and reverse direction at platform edges 
                    if (!playerIsClose && (paintCanMotion.position.x <= platformLeftEdge + paintCanMotion.scale.x ||
                        paintCanMotion.position.x >= platformRightEdge - paintCanMotion.scale.x)) {
                        paintCanMotion.velocity.x *= -1;
                    }

                    if (paintCanMotion.velocity.x == 0) {
                        paintCanMotion.velocity.x = (rand() % 2) == 0 ? -200.f : 200.f;
                    }
                    break;
                }
            }
        }
    }
}

bool AISystem::rectangleCollides(const Motion& motion1, const Motion& motion2) {
    bool y_val = (motion1.position[1] - abs(motion1.scale.y) / 2.f) < (motion2.position[1] + abs(motion2.scale.y) / 2.f) &&
        (motion2.position[1] - abs(motion2.scale.y) / 2.f) < (motion1.position[1] + abs(motion1.scale.y) / 2.f);
    bool x_val = (motion1.position[0] - abs(motion1.scale.x / 2.f)) < (motion2.position[0] + abs(motion2.scale.x) / 2.f) &&
        (motion2.position[0] - abs(motion2.scale.x / 2.f) < (motion1.position[0] + abs(motion1.scale.x) / 2.f));
    return y_val && x_val;
}