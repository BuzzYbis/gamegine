// engine.cpp                                                         -*-C++-*-
#include <core/engine.h>

// core
#include <core/step_timer.h>

// scene
#include <scene/components/mesh_component.h>
#include <scene/entity.h>
#include <scene/scene.h>

// ui
#include <ui/panels/entities_panel.h>
#include <ui/panels/game_panel.h>

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

namespace engine::core {
void Engine::initialize(const char* title, const int width, const int height)
{
    // Create platform
    d_window = std::make_unique<Window>();
    d_window->initialize(title, width, height);

    // Initialize renderer
    d_renderer = std::make_unique<renderer::Renderer>(&*d_window);
    d_renderer->initialize(enableValidationLayers);

    d_assetManager = std::make_unique<asset::AssetManager>(
        d_renderer->context(),
        d_renderer->swapchain()->format(),
        d_renderer->swapchain()->depthFormat(),
        d_renderer->descriptorSetLayout());
    d_scene         = std::make_unique<scene::Scene>();
    d_systemManager = std::make_unique<scene::SystemManager>();

    d_inputManager = std::make_unique<InputManager>();
    // Setup binding keys
    d_inputManager->bindKey(GLFW_KEY_W, InputAction::MOVE_FORWARD);
    d_inputManager->bindKey(GLFW_KEY_S, InputAction::MOVE_BACKWARD);
    d_inputManager->bindKey(GLFW_KEY_A, InputAction::MOVE_LEFT);
    d_inputManager->bindKey(GLFW_KEY_D, InputAction::MOVE_RIGHT);
    d_inputManager->bindKey(GLFW_KEY_Z, InputAction::MOVE_UP);
    d_inputManager->bindKey(GLFW_KEY_X, InputAction::MOVE_DOWN);
    d_inputManager->bindKey(GLFW_MOUSE_BUTTON_RIGHT, InputAction::RIGHT_CLICK);

    // Create ui context
    d_uiManager = std::make_unique<ui::UIManager>();
    d_uiManager->initialize(d_renderer->context(),
                            *d_renderer->swapchain(),
                            *d_window);
    d_uiManager->addPanel(std::make_unique<ui::EntitiesPanel>(d_scene.get()));
    d_uiManager->addPanel(
        std::make_unique<ui::GamePanel>(d_renderer.get(),
                                        d_inputManager.get()));
}

void Engine::run()
{
    d_timer.reset();

    while (!d_window->shouldClose()) {
        d_timer.tick();
        const float dt = d_timer.releaseDeltaTime();

        d_window->pollEvents();
        d_inputManager->update(d_window.get(), dt);

        // Setup UI
        d_uiManager->beginFrame();

        // Game logic (ECS)
        d_systemManager->updateAll(d_scene->registry(), *d_inputManager, dt);

        d_uiManager->renderPanels();

        vk::CommandBuffer cmd = d_renderer->beginFrame(*d_scene);

        if (cmd) {
            // Render scene
            d_renderer->renderScene(cmd, *d_scene);

            d_renderer->beginSwapchainPass(cmd);

            // Render UI
            d_uiManager->endFrame(cmd);

            // Submit and present
            d_renderer->endFrame(cmd);
        }
        else {
            d_uiManager->cleanUpFrame();
        }
    }
}

void Engine::clean()
{
    if (d_renderer) {
        d_renderer->context().device().waitIdle();
    }

    if (d_uiManager) {
        d_uiManager->shutdown();
        d_uiManager.reset();
    }

    d_assetManager.reset();
    d_scene.reset();
    d_renderer.reset();
    d_window.reset();
}

scene::Entity Engine::createEntity(const std::string& name) const
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

}  // close core namespace
