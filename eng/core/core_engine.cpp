// core_engine.cpp                                                    -*-C++-*-
#include <core/core_engine.h>

// core
#include <core/core_profiler.h>
#include <core/core_steptimer.h>

// rhi
#include <rhi/rhi_factory.h>

// scn
#include <scn/comp/comp_mesh.h>
#include <scn/scn_entity.h>
#include <scn/scn_scene.h>

// ui
#include <ui/pnl/pnl_profiler.h>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

namespace eng::core {

void Engine::initialize(const char* title, const int width, const int height)
{
    d_window = std::make_unique<Window>();
    d_window->initialize(title, width, height);

    constexpr auto api = rhi::GraphicsAPI::Vulkan;
    d_rhiContext       = rhi::Factory::createContext(api, d_window.get());

    if (!d_rhiContext->initialize(enableValidationLayers)) {
        throw std::runtime_error("Failed to initialize the RHI context!");
    }

    d_renderer = std::make_unique<rnd::Renderer>(d_rhiContext.get(),
                                                 d_window.get());
    d_renderer->initialize();

    d_assetManager = std::make_unique<asset::AssetManager>(d_rhiContext.get(),
                                                           d_renderer.get());

    d_scene         = std::make_unique<scn::Scene>();
    d_systemManager = std::make_unique<scn::SystemManager>();
    d_inputManager  = std::make_unique<InputManager>();

    // Setup binding keys
    d_inputManager->bindKey(GLFW_KEY_W, InputAction::MOVE_FORWARD);
    d_inputManager->bindKey(GLFW_KEY_S, InputAction::MOVE_BACKWARD);
    d_inputManager->bindKey(GLFW_KEY_A, InputAction::MOVE_LEFT);
    d_inputManager->bindKey(GLFW_KEY_D, InputAction::MOVE_RIGHT);
    d_inputManager->bindKey(GLFW_KEY_Z, InputAction::MOVE_UP);
    d_inputManager->bindKey(GLFW_KEY_X, InputAction::MOVE_DOWN);
    d_inputManager->bindKey(GLFW_MOUSE_BUTTON_RIGHT, InputAction::RIGHT_CLICK);

    d_uiManager = std::make_unique<ui::UIManager>();
    d_uiManager->initialize(&d_renderer->context(),
                            d_renderer->swapchain(),
                            *d_window,
                            api);

    d_uiManager->addPanel(
        std::make_unique<ui::ProfilerPanel>(&Profiler::instance()));
}

void Engine::run()
{
    d_timer.reset();

    Profiler& profiler = Profiler::instance();

    while (!d_window->shouldClose()) {
        d_timer.tick();
        const float dt = d_timer.releaseDeltaTime();

        profiler.beginFrame();

        // The frame is timed here rather than taken from the step timer, so
        // that the total and the scopes that break it down describe one and
        // the same frame.
        const auto frameStart = StepTimer::Clock::now();

        {
            ENG_PROFILE_SCOPE("Input");
            d_window->pollEvents();
            d_inputManager->update(d_window.get(), dt);
        }

        // Setup UI
        {
            ENG_PROFILE_SCOPE("UI Begin");
            d_uiManager->beginFrame();
        }

        // Game logic (ECS)
        {
            ENG_PROFILE_SCOPE("ECS Update");
            d_systemManager->updateAll(d_scene->registry(),
                                       *d_inputManager,
                                       dt);
        }

        {
            ENG_PROFILE_SCOPE("UI Panels");
            d_uiManager->renderPanels();
        }

        // The image acquisition and the fence wait live here: keeping them
        // in a scope of their own is what tells a stalled CPU apart from a
        // saturated GPU.
        rhi::CommandListProtocol* cmd = nullptr;
        {
            ENG_PROFILE_SCOPE("GPU Wait");
            cmd = d_renderer->beginFrame(*d_scene);
        }

        if (cmd) {
            {
                ENG_PROFILE_SCOPE("Record");

                // 1. Start swapchain rendering pass
                d_renderer->beginSwapchainPass(cmd);

                // 2. Render scene directly into the swapchain
                d_renderer->renderScene(cmd, *d_scene);
            }

            // 3. Render UI into the swapchain pass
            {
                ENG_PROFILE_SCOPE("UI Draw");
                d_uiManager->endFrame(cmd);
            }

            // 4. Submit and present
            {
                ENG_PROFILE_SCOPE("Submit");
                d_renderer->endFrame(cmd);
            }
        }
        else {
            d_uiManager->cleanUpFrame();
        }

        ENG_PROFILE_COUNTER("Entities",
                            static_cast<int64_t>(d_scene->registry().alive()));

        const std::chrono::duration<float, std::milli> frameMs =
            StepTimer::Clock::now() - frameStart;

        profiler.endFrame(frameMs.count());
    }
}

void Engine::clean()
{
    if (d_rhiContext) {
        d_rhiContext->waitIdle();
    }

    if (d_uiManager) {
        d_uiManager->shutdown();
        d_uiManager.reset();
    }

    d_assetManager.reset();
    d_scene.reset();
    d_renderer.reset();
    d_rhiContext.reset();
    d_window.reset();
}

scn::Entity Engine::createEntity(const std::string& name) const
{
    if (!d_scene) {
        throw std::runtime_error("[Engine::createEntity] Scene is null!");
    }
    return d_scene->createEntity(name);
}

Engine::~Engine()
{
    clean();
}

}  // close package namespace