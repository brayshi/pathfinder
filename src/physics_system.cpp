// internal
#include "physics_system.hpp"
#include "world_init.hpp"
#include <iostream>
#include <stack>
#include <cfloat>

void PhysicsSystem::step(float elapsed_ms)
{
	//elapsed_ms = clamp(elapsed_ms, 0.f, 8.f);
	auto& motion_registry = registry.motions;
	for (uint i = 0; i < motion_registry.size(); i++)
	{
		Motion& motion = motion_registry.components[i];
		if (motion.fixed) {
			continue;
		}
		float step_seconds = elapsed_ms / 1000.f;
		Entity entity = motion_registry.entities[i];

		// decide y accel and vel
		if (motion.isJumping) {
			printf("%f\n", motion.timeJumping);
			if (motion.timeJumping <= 150.f) {
				motion.grounded = false;
				motion.velocity.y = -jump_height;
				motion.acceleration.y = 0;
				motion.timeJumping += elapsed_ms;
			}
			else {
				motion.isJumping = false;
			}
		}
		else if (motion.grounded) {
			motion.jumpsLeft = 1;
			motion.acceleration.y = 0.f;
			motion.velocity.y = 0.f;
		}
		else if (registry.boulders.has(entity)) {
			// this should just be set once in the boulder creation
			motion.acceleration.y = gravity / 20;
			motion.velocity.y = clamp(motion.velocity.y + motion.acceleration.y, -terminal_velocity, terminal_velocity);
		}
		else if (!motion.notAffectedByGravity) {
			motion.acceleration.y = gravity;
			motion.velocity.y = clamp(motion.velocity.y + motion.acceleration.y, -terminal_velocity, terminal_velocity);
		}
    
		if (registry.players.has(entity)) {
			checkWindowBoundary(motion);
			applyFriction(motion);
			if (registry.deathTimers.has(entity)) {
				motion.velocity.x = 0.f;
			}
		}
		updatePaintCanGroundedState();

		motion.position += motion.velocity * step_seconds;
	}

	// Check for collisions between all moving entities
    ComponentContainer<Motion> &motion_container = registry.motions;
	for(uint i = 0; i<motion_container.components.size(); i++)
	{
		Motion& motion_i = motion_container.components[i];
		Entity entity_i = motion_container.entities[i];
		
		// note starting j at i+1 to compare all (i,j) pairs only once (and to not compare with itself)
		for(uint j = i+1; j<motion_container.components.size(); j++)
		{
			Motion& motion_j = motion_container.components[j];
			Entity entity_j = motion_container.entities[j];

			if (collisionSystem.collides(motion_i, entity_i, motion_j, entity_j)) {
				registry.collisions.emplace_with_duplicates(entity_i, entity_j);
				registry.collisions.emplace_with_duplicates(entity_j, entity_i);
			}
		}
	}

	ComponentContainer<Platform>& platform_container = registry.platforms;
	//ComponentContainer<Motion>& motion_container = registry.motions;
	Motion& player = motion_container.get(registry.players.entities[0]);
	bool touching_any_platform = false;
	for (uint i = 0; i < platform_container.components.size(); i++) {
		Motion& platform = motion_container.get(platform_container.entities[i]);
		if (collisionSystem.rectangleCollides(platform, player)) {
			touching_any_platform = true;
			break;
		}
	}
	player.grounded = touching_any_platform;
}

void PhysicsSystem::updatePaintCanGroundedState() {
	auto& paintCanRegistry = registry.paintCans;
	auto& platformContainer = registry.platforms;

	for (auto& paintCanEntity : paintCanRegistry.entities) {
		Motion& paintCanMotion = registry.motions.get(paintCanEntity);
		bool isTouchingPlatform = false;

		for (auto& platformEntity : platformContainer.entities) {
			Motion& platformMotion = registry.motions.get(platformEntity);

			if (collisionSystem.rectangleCollides(paintCanMotion, platformMotion)) {
				isTouchingPlatform = true;
				paintCanMotion.grounded = true;
				paintCanMotion.velocity.y = 0;


				float platformTop = platformMotion.position.y + platformMotion.scale.y / 2.0f;
				paintCanMotion.position.y = platformTop - paintCanMotion.scale.y / 2.0f;
				break; 
			}
		}

		if (!isTouchingPlatform) {
			paintCanMotion.grounded = false;
		}
	}
}

// used to keep player within window boundary
// if entity hits top, left, or right, change velocity so the entity bounces off boundary
void PhysicsSystem::checkWindowBoundary(Motion& motion) {
	if (motion.position.x - abs(motion.scale.x) / 2 < 0) {
		motion.position.x += 1.f;
		motion.velocity.x = 0.f;
	}
	else if (motion.position.x + abs(motion.scale.x) / 2 > window_width_px) {
		motion.position.x -= 1.f;
		motion.velocity.x = 0.f;
	}
	else if (motion.position.y - abs(motion.scale.y) / 2 < 0) {
		motion.position.y += 1.f;
		motion.velocity.y = 0.f;
	}
}

void PhysicsSystem::applyFriction(Motion& motion) {
	// apply horizontal friction to player
	if (motion.velocity.x != 0.0 && motion.acceleration.x != 0.0) {
		// this conditional ASSUMES we are decelerating due to friction, and should stop at 0
		if (abs(motion.velocity.x) < abs(motion.acceleration.x)) {
			motion.velocity.x = 0.0;
		}
		else {
			motion.velocity.x = clamp(motion.velocity.x + motion.acceleration.x, -terminal_velocity, terminal_velocity);
		}
	}
	else {
		motion.acceleration.x = 0.0f;
	}
}