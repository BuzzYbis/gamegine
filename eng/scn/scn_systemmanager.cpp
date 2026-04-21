// system_manager.cpp                                                 -*-C++-*-
#include <scn/scn_systemmanager.h>

namespace eng::scn {

void SystemManager::registerSystem(std::unique_ptr<sys::ISystem> system)
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

}  // close package namespace
