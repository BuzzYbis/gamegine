// main.cpp                                                           -*-C++-*-

// std
#include <iostream>
#include <memory>
#include <string>

// core
#include <core/core_engine.h>

// scene
#include <scn/scn_component.h>
#include <scn/sys/sys_camera.h>

using namespace eng::scn;
using namespace eng::rnd;

constexpr int WINDOW_WIDTH  = 800;
constexpr int WINDOW_HEIGHT = 600;

int main()
{
    const auto engine = std::make_unique<eng::core::Engine>();

    try {
        engine->initialize("Gamegine", WINDOW_WIDTH, WINDOW_HEIGHT);

        engine->systemManager()->registerSystem(
            std::make_unique<sys::CameraSystem>());

        Entity cameraEntity = engine->createEntity("Camera");

        auto& camera = cameraEntity.addComponent<comp::CameraComponent>();
        camera.aspectRatio = static_cast<float>(WINDOW_WIDTH) /
                             static_cast<float>(WINDOW_HEIGHT);
        camera.farPlane = 1000.0f;
        auto& cameraTransform =
            cameraEntity.getComponent<comp::TransformComponent>();
        cameraTransform.setPosition({-2.5f, 0.25f, 0.0f});
        cameraTransform.setRotation({0.0f, 0.0f, 0.0f});

        const auto [meshes, materials] = engine->assetManager()->loadMesh(
            "models/viking_room.obj");

        auto entity      = engine->createEntity("Viking_room");
        auto& [meshCompMesh,
               material] = entity.addComponent<comp::MeshComponent>();
        meshCompMesh     = meshes;
        material         = materials;

        auto& entityTransform =
            entity.getComponent<comp::TransformComponent>();
        entityTransform.setPosition({0.0f, -0.2f, 0.0f});
        entityTransform.setRotation(
            {glm::radians(-90.0f), 0.0f, glm::radians(180.0f)});

        engine->run();
    }
    catch (const std::exception& e) {
        std::cerr << "An exception occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
