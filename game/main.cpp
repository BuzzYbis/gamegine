// main.cpp                                                           -*-C++-*-

#include <core/engine.h>
#include <iostream>
#include <memory>
#include <renderer/renderer.h>
#include <scene/components.h>
#include <scene/systems/camera_system.h>
#include <string>

using namespace engine::scene;
using namespace engine::renderer;

constexpr int WINDOW_WIDTH  = 800;
constexpr int WINDOW_HEIGHT = 600;

int main()
{
    const auto engine = std::make_unique<engine::core::Engine>();

    try {
        engine->initialize("Gamegine", WINDOW_WIDTH, WINDOW_HEIGHT);

        engine->systemManager()->registerSystem(
            std::make_unique<CameraSystem>());

        Entity cameraEntity = engine->createEntity("Camera");

        auto& camera       = cameraEntity.addComponent<CameraComponent>();
        camera.aspectRatio = static_cast<float>(WINDOW_WIDTH) /
                             static_cast<float>(WINDOW_HEIGHT);
        camera.farPlane = 1000.0f;
        auto& cameraTransform =
            cameraEntity.getComponent<TransformComponent>();
        cameraTransform.setPosition({0.0f, 0.0f, 0.0f});
        cameraTransform.setRotation({0.0f, 0.0f, 0.0f});

        const auto [meshes, materials] = engine->assetManager()->loadMesh(
            "models/viking_room.obj");

        auto entity                    = engine->createEntity("Viking_room");
        auto& [meshCompMesh, material] = entity.addComponent<MeshComponent>();
        meshCompMesh                   = meshes;
        material                       = materials;

        engine->run();
    }
    catch (const std::exception& e) {
        std::cerr << "An exception occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
