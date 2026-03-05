// system_manager.cpp                                               -*-C++-*-
#include "system_manager.h"

namespace engine::scene {

void SystemManager::registerSystem(std::unique_ptr<ISystem> system)
{
    d_systems.push_back(std::move(system));
}

void SystemManager::updateAll(entt::registry&     registry,
                              core::InputManager& input,
                              const float         dt) const
{
    for (const auto& system : d_systems) {
        system->update(registry, input, dt);
    }
}

}  // close engine::scene namespace
