// system_manager.cpp                                                 -*-C++-*-
#include <scn/scn_systemmanager.h>

namespace eng::scn {

void SystemManager::registerSystem(std::unique_ptr<sys::ISystem> system)
{
    d_scopeIds.push_back(
        core::Profiler::instance().registerScope(system->name()));
    d_systems.push_back(std::move(system));
}

void SystemManager::updateAll(entt::registry&     registry,
                              core::InputManager& input,
                              const float         dt) const
{
    for (size_t i = 0; i < d_systems.size(); ++i) {
        ENG_PROFILE_SCOPE_ID(d_scopeIds[i]);
        d_systems[i]->update(registry, input, dt);
    }
}

}  // close package namespace
