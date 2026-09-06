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
        camera.farPlane = 1000.0F;
        camera.fov      = 90;

        auto& cameraTransform =
            cameraEntity.getComponent<comp::TransformComponent>();
        cameraTransform.setPosition({0.0F, 0.0F, 0.0F});
        cameraTransform.setRotation({0.F, 0.0F, 0.0F});

        const auto [meshes, materials] = engine->assetManager()->loadMesh(
            //    "models/StainedGlassLamp/glTF-KTX-BasisU/StainedGlassLamp.gltf");
            //    "models/AnisotropyBarnLamp/glTF-KTX-BasisU/"
            //    "AnisotropyBarnLamp.gltf");
            //    "models/glTF/DamagedHelmet.gltf");
            //"models/chest/chest.glb");
            "models/bistro/bistro.gltf");

        auto entity      = engine->createEntity("DamagedHelmet (gltf)");
        auto& [meshCompMesh,
               material] = entity.addComponent<comp::MeshComponent>();
        meshCompMesh     = meshes;
        material         = materials;

        auto& entityTransform =
            entity.getComponent<comp::TransformComponent>();
        entityTransform.setScale({10.0F, 10.0F, 10.0F});
        entityTransform.setPosition({1.5F, -0.3F, 0.0F});
        entityTransform.setRotation(
            {glm::radians(0.0F), glm::radians(0.0F), glm::radians(0.0F)});

        engine->run();
    }
    catch (const std::exception& e) {
        std::cerr << "An exception occurred: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
