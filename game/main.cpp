// main.cpp                                                           -*-C++-*-

// std
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// asset
#include <asset/asset_benchmark.h>

// core
#include <core/core_engine.h>

// scene
#include <scn/scn_component.h>
#include <scn/sys/sys_camera.h>

using namespace eng::scn;
using namespace eng::rnd;

constexpr int WINDOW_WIDTH  = 800;
constexpr int WINDOW_HEIGHT = 600;

constexpr const char* SCENE_PATH = "models/bistro/bistro.gltf";

namespace {

/// Command line of the example application.
struct Options {
    /// Whether to measure the loading of the scene instead of running it.
    bool benchmark = false;

    /// Parameters of that measurement.
    eng::asset::LoadBenchmark::Config bench;
};

/// Return the options described by the specified 'argc' and 'argv'.
Options parseOptions(const int argc, char** argv)
{
    Options options;
    options.bench.scenePath = SCENE_PATH;

    const std::vector<std::string> args(argv + 1, argv + argc);

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& arg = args[i];

        if (arg == "--bench") {
            options.benchmark = true;
        }
        else if (arg == "--runs" && i + 1 < args.size()) {
            options.bench.runs = static_cast<uint32_t>(std::stoul(args[++i]));
        }
        else if (arg == "--scene" && i + 1 < args.size()) {
            options.bench.scenePath = args[++i];
        }
        else if (arg == "--csv" && i + 1 < args.size()) {
            options.bench.csvPath = args[++i];
        }
        else {
            std::cerr << "Usage: example_gamegine [--bench [--runs N] "
                         "[--scene PATH] [--csv PATH]]\n";
        }
    }

    return options;
}

}  // close unnamed namespace

int main(int argc, char** argv)
{
    const Options options = parseOptions(argc, argv);

    const auto engine = std::make_unique<eng::core::Engine>();

    try {
        engine->initialize("Gamegine", WINDOW_WIDTH, WINDOW_HEIGHT);

        // Measuring the load needs a live device, but nothing of the scene
        // that would normally be built on top of it.
        if (options.benchmark) {
            const bool ok = eng::asset::LoadBenchmark::run(
                options.bench,
                &engine->renderer()->context(),
                engine->renderer());

            return ok ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        engine->systemManager()->registerSystem(
            std::make_unique<sys::CameraSystem>());

        Entity cameraEntity = engine->createEntity("Camera");

        auto& camera = cameraEntity.addComponent<comp::CameraComponent>();
        camera.aspectRatio = static_cast<float>(WINDOW_WIDTH) /
                             static_cast<float>(WINDOW_HEIGHT);
        camera.farPlane    = 1000.0F;
        camera.fov         = 90;

        auto& cameraTransform =
            cameraEntity.getComponent<comp::TransformComponent>();
        cameraTransform.setPosition({0.0F, 0.0F, 0.0F});
        cameraTransform.setRotation({0.F, 0.0F, 0.0F});

        const eng::asset::LoadedModel model = engine->assetManager()->loadMesh(
            //    "models/StainedGlassLamp/glTF-KTX-BasisU/StainedGlassLamp.gltf");
            //    "models/AnisotropyBarnLamp/glTF-KTX-BasisU/"
            //    "AnisotropyBarnLamp.gltf");
            //    "models/glTF/DamagedHelmet.gltf");
            //    "models/chest/chest.glb");
            SCENE_PATH);

        auto  entity     = engine->createEntity("DamagedHelmet (gltf)");
        auto& meshComp   = entity.addComponent<comp::MeshComponent>();
        meshComp.batches = engine->assetManager()->buildMeshBatches(model);

        auto& entityTransform =
            entity.getComponent<comp::TransformComponent>();
        entityTransform.setScale({1.0F, 1.0F, 1.0F});
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
