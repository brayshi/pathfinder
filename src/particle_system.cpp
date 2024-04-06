
#include "common.hpp"
#include "tiny_ecs.hpp"
#include "components.hpp"
#include "tiny_ecs_registry.hpp"
#include <particle_system.hpp>


void ParticleSystem::step(float elapsed_ms) {
	// Update particle positions
	float step_sceond = elapsed_ms / 1000.f;

	auto& particles_container = registry.particles;
	std::vector<Entity> to_be_removed;

	for (auto& particleEntity : particles_container.entities) {
		auto& particle = particles_container.get(particleEntity);
		auto& motion = registry.motions.get(particleEntity);

		motion.position += motion.velocity * step_sceond;
		particle.life -= step_sceond;

		if (particle.life <= 0.0f) {
			to_be_removed.push_back(particleEntity);
		}
	}

	for (auto& particle : to_be_removed) {
		registry.remove_all_components_of(particle);
	}

	auto& emtitter_container = registry.particleEmitters;

	for (auto& emitterEntity : emtitter_container.entities) {
		auto& emitter = emtitter_container.get(emitterEntity);
		int particlesToSpawn = emitter.particles_per_second * step_sceond;

		for (int i = 0; i < particlesToSpawn; i++) {
			spawn_particle(emitter);
		}
	}

}

void ParticleSystem::spawn_particle(const ParticleEmitter& emitter) {
	Entity entity = Entity();
	auto& particle = registry.particles.emplace(entity);
	particle.color = emitter.color;
	particle.life = emitter.life_span;

	auto& motion = registry.motions.emplace(entity);

	motion.poistion = emitter.emission_point;
	motion.velocity = emitter.initial_velocity;

	registry.renderRequests.insert(
		entity,
		{ TEXTURE_ASSET_ID::CIRCLEPARTICLE,
				 EFFECT_ASSET_ID::TEXTURED,
				 GEOMETRY_BUFFER_ID::SPRITE });
}