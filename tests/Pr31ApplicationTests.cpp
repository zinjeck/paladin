#include "TestFramework.h"
#include "core/Application.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "platform/Window.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <iostream>
namespace Paladin
{
    struct ApplicationSmokeTest
    {
        static void capture(Application& app, const char* name)
        {
            const char* root = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS");
            if (!root) return;
            std::filesystem::create_directories(root);
            auto* surface = SDL_RenderReadPixels(SDL_GetRenderer(app.window_->nativeHandle()), nullptr);
            PALADIN_CHECK(surface);
            const bool saved = IMG_SavePNG(surface, (std::filesystem::path(root) / name).string().c_str());
            SDL_DestroySurface(surface);
            PALADIN_CHECK(saved);
        }
        static void startup(Application& app)
        {
            PALADIN_CHECK(app.startupReady_ && !app.startupCancelled_ && app.startupError_.empty());
            PALADIN_CHECK(app.startupProgress_ == 1 && app.startupProgressFrames_ >= 3);
            PALADIN_CHECK(!app.simulation_ && !app.worldRenderer_ && !app.cityRenderer_);
            capture(app, "pr31-startup-ready.png");
            const auto manager = app.renderer_->compiledAssets();
            PALADIN_CHECK(!manager->records().empty() && manager->uploads.empty());
            for (const auto& record : manager->records())
                PALADIN_CHECK(manager->residency(record.id) && manager->residency(record.id)->state == AssetState::Resident);
            const auto bytes = manager->residentGpuBytes();
            const auto cache = app.renderer_->sceneSpriteCache();
            PALADIN_CHECK(cache && cache->ready() && bytes > 0);
            SceneSpriteLibrary second;
            second.load(*app.renderer_, (std::filesystem::path(SDL_GetBasePath()) / "assets/sprites").string());
            PALADIN_CHECK(second.ready());
            const auto* first = cache->find("citizen.militia.male.front.walk");
            const auto* reused = second.find("citizen.militia.male.front.walk");
            PALADIN_CHECK(first && reused && first->texture == reused->texture);
            PALADIN_CHECK(first->selectionSilhouette == reused->selectionSilhouette);
            PALADIN_CHECK(manager == app.renderer_->compiledAssets() && manager->residentGpuBytes() == bytes);
            std::cout << "[pr31/startup] real progress, every packaged asset resident, shared GPU/silhouette cache, no generated world passed\n";
        }
        static void run()
        {
            Application app;
            PALADIN_CHECK(app.renderer_ && app.renderer_->isValid());
            startup(app);
        }
    };
}
int main()
{
    try { Paladin::ApplicationSmokeTest::run(); return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
