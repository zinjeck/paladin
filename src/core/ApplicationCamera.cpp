#include "core/Application.h"
#include "interaction/GlobeCameraNavigation.h"
#include "interaction/SettlementPlacementController.h"
#include "platform/Window.h"
#include "rendering/Camera2D.h"
#include "rendering/GlobeView.h"
#include "rendering/Renderer.h"
#include "rendering/TileRenderMetrics.h"
#include "rendering/WorldRenderer.h"
#include "simulation/Simulation.h"
#include "ui/CityHud.h"
#include "ui/DebugConsole.h"
#include "ui/EmploymentPanel.h"
#include "ui/FoundingPanel.h"
#include "ui/LedgerPanel.h"
#include "ui/SettlementInspectionPanel.h"
#include "ui/SimulationSpeedControls.h"
#include "ui/WorldHud.h"
#include "world/World.h"
#include "world/settlements/SettlementMap.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

namespace Paladin
{
    void Application::updateCameraMovement(double frameDeltaSeconds)
    {
        if (debugConsole_->wantsKeyboard())
        {
            return;
        }
        const bool* keyboardState = SDL_GetKeyboardState(nullptr);

        double directionX = 0.0;
        double directionY = 0.0;

        if (keyboardState[SDL_SCANCODE_A])
        {
            directionX -= 1.0;
        }

        if (keyboardState[SDL_SCANCODE_D])
        {
            directionX += 1.0;
        }

        if (keyboardState[SDL_SCANCODE_W])
        {
            directionY -= 1.0;
        }

        if (keyboardState[SDL_SCANCODE_S])
        {
            directionY += 1.0;
        }

        const double baseTilePixels =
            tileRenderMetrics_ ? std::max(tileRenderMetrics_->tilePixels, 0.001)
                               : 1.0;

        double panSpeedTilesPerSecondAtZoomOne =
            cameraNavigationPolicy_.keyboardPanSpeedScreenPixelsPerSecond /
            baseTilePixels;

        const bool keyboardMoving = directionX != 0.0 || directionY != 0.0;

        if (keyboardMoving)
        {
            edgeScrollDwellSeconds_ = 0.0;
        }
        else
        {
            float mouseX = 0.0F;
            float mouseY = 0.0F;

            const SDL_MouseButtonFlags mouseButtons =
                SDL_GetMouseState(&mouseX, &mouseY);

            const SDL_WindowFlags windowFlags =
                SDL_GetWindowFlags(window_->nativeHandle());

            const float viewportWidth =
                static_cast<float>(renderer_->outputWidth());

            const float viewportHeight =
                static_cast<float>(renderer_->outputHeight());

            const bool edgeScrollEligible =
                (windowFlags & SDL_WINDOW_INPUT_FOCUS) != 0 &&
                (windowFlags & SDL_WINDOW_MOUSE_FOCUS) != 0 &&
                mouseButtons == 0 && mouseX >= 0.0F && mouseY >= 0.0F &&
                mouseX < viewportWidth && mouseY < viewportHeight &&
                !activeHudContainsPoint(mouseX, mouseY);

            if (edgeScrollEligible)
            {
                const float activationWidth = std::min(
                    cameraNavigationPolicy_.edgeActivationWidthPixels,
                    std::min(viewportWidth, viewportHeight) * 0.5F
                );

                const auto edgeAxis =
                    [this, activationWidth](float position, float extent)
                {
                    double intensity = 0.0;

                    if (position < activationWidth)
                    {
                        intensity =
                            -(1.0 -
                              static_cast<double>(position) / activationWidth);
                    }
                    else if (position > extent - activationWidth)
                    {
                        intensity =
                            1.0 - static_cast<double>(extent - position) /
                                      activationWidth;
                    }

                    return std::copysign(
                        std::pow(
                            std::abs(intensity),
                            cameraNavigationPolicy_.edgeResponseExponent
                        ),
                        intensity
                    );
                };

                directionX = edgeAxis(mouseX, viewportWidth);
                directionY = edgeAxis(mouseY, viewportHeight);
            }

            if (directionX != 0.0 || directionY != 0.0)
            {
                edgeScrollDwellSeconds_ += frameDeltaSeconds;

                if (edgeScrollDwellSeconds_ <
                    cameraNavigationPolicy_.edgeActivationDelaySeconds)
                {
                    return;
                }

                panSpeedTilesPerSecondAtZoomOne =
                    cameraNavigationPolicy_.edgePanSpeedScreenPixelsPerSecond /
                    baseTilePixels;
            }
            else
            {
                edgeScrollDwellSeconds_ = 0.0;
                return;
            }
        }

        double directionLength = std::hypot(directionX, directionY);

        if (directionLength == 0.0)
        {
            return;
        }

        if (directionLength > 1.0)
        {
            directionX /= directionLength;
            directionY /= directionLength;
            directionLength = 1.0;
        }

        if (screen_ == Screen::World && worldRenderer_->globeEnabled)
        {
            GlobeCameraNavigation::pan(
                *camera_,
                simulation_->world().grid(),
                renderer_->outputWidth(),
                renderer_->outputHeight(),
                directionX,
                directionY,
                panSpeedTilesPerSecondAtZoomOne * baseTilePixels *
                    frameDeltaSeconds
            );
            return;
        }
        const double panSpeedTilesPerSecond =
            panSpeedTilesPerSecondAtZoomOne / camera_->zoom();

        camera_->move(
            directionX * panSpeedTilesPerSecond * frameDeltaSeconds,
            directionY * panSpeedTilesPerSecond * frameDeltaSeconds
        );

        clampCameraToWorld();
    }

    void Application::updateCameraZoom(double frameDeltaSeconds)
    {
        if (debugConsole_->wantsKeyboard())
        {
            return;
        }
        const bool* keyboardState = SDL_GetKeyboardState(nullptr);

        double zoomDirection = 0.0;

        if (keyboardState[SDL_SCANCODE_EQUALS] ||
            keyboardState[SDL_SCANCODE_KP_PLUS])
        {
            zoomDirection += 1.0;
        }

        if (keyboardState[SDL_SCANCODE_MINUS] ||
            keyboardState[SDL_SCANCODE_KP_MINUS])
        {
            zoomDirection -= 1.0;
        }

        if (zoomDirection == 0.0)
        {
            return;
        }

        float mouseX = 0.0F;
        float mouseY = 0.0F;

        SDL_GetMouseState(&mouseX, &mouseY);

        constexpr double keyboardZoomFactorPerSecond = 2.0;

        applyCameraZoom(
            std::pow(
                keyboardZoomFactorPerSecond,
                zoomDirection * frameDeltaSeconds
            ),
            static_cast<double>(mouseX),
            static_cast<double>(mouseY)
        );
    }

    void Application::updateSettlementPlacementHover(
        double screenX,
        double screenY
    )
    {
        if (screen_ == Screen::World)
        {
            const auto& g = simulation_->world().grid();
            auto hit = WorldMapNavigation::pick(
                *camera_,
                g,
                renderer_->outputWidth(),
                renderer_->outputHeight(),
                tileRenderMetrics_->scaledTilePixels(camera_->zoom()),
                worldRenderer_->globeEnabled,
                screenX,
                screenY
            );
            if (activeHudContainsPoint(float(screenX), float(screenY)))
            {
                hit.reset();
            }
            settlementPlacementController_->setHoveredPosition(
                hit ? std::optional<
                          WorldTilePosition>{{std::clamp(int(hit->u * g.width()), 0, g.width() - 1), std::clamp(int(hit->v * g.height()), 0, g.height() - 1)}}
                    : std::nullopt
            );
            return;
        }
        const double tilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());

        const double viewportWidth =
            static_cast<double>(renderer_->outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer_->outputHeight());

        if (tilePixels <= 0.0 || screenX < 0.0 || screenY < 0.0 ||
            screenX >= viewportWidth || screenY >= viewportHeight)
        {
            settlementPlacementController_->setHoveredPosition(std::nullopt);

            return;
        }

        const double worldTileX =
            camera_->tileX() + (screenX - viewportWidth * 0.5) / tilePixels;

        const double worldTileY =
            camera_->tileY() + (screenY - viewportHeight * 0.5) / tilePixels;

        const WorldTilePosition position{
            static_cast<std::int32_t>(std::floor(worldTileX)),
            static_cast<std::int32_t>(std::floor(worldTileY))
        };

        if (!simulation_->world().grid().isValidPosition(
                {position.x, position.y}
            ))
        {
            settlementPlacementController_->setHoveredPosition(std::nullopt);

            return;
        }

        settlementPlacementController_->setHoveredPosition(position);
    }

    void Application::applyCameraZoom(
        double multiplier,
        double screenX,
        double screenY
    )
    {
        if (multiplier <= 0.0 || multiplier == 1.0)
        {
            return;
        }

        const double viewportWidth =
            static_cast<double>(renderer_->outputWidth());

        const double viewportHeight =
            static_cast<double>(renderer_->outputHeight());

        if (viewportWidth <= 0.0 || viewportHeight <= 0.0)
        {
            return;
        }

        if (screen_ == Screen::World && worldRenderer_->globeEnabled)
        {
            camera_->setZoom(
                std::clamp(camera_->zoom() * multiplier, .65, 24.)
            );
            return;
        }
        const double screenOffsetX = screenX - viewportWidth * 0.5;

        const double screenOffsetY = screenY - viewportHeight * 0.5;

        const double oldTilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());

        if (oldTilePixels <= 0.0)
        {
            return;
        }

        const double worldTileXUnderCursor =
            camera_->tileX() + screenOffsetX / oldTilePixels;

        const double worldTileYUnderCursor =
            camera_->tileY() + screenOffsetY / oldTilePixels;

        if (screen_ == Screen::World)
        {
            camera_->setWorldZoom(
                std::clamp(
                    camera_->zoom() * multiplier,
                    viewportWidth / (2. * simulation_->world().grid().width() *
                                     tileRenderMetrics_->tilePixels),
                    80.
                )
            );
        }
        else
        {
            const auto* map =
                simulation_->settlementMap(activeCitySettlementId_);
            const double fit = map ? std::min(
                                         viewportWidth / map->grid().width(),
                                         viewportHeight / map->grid().height()
                                     ) / (tileRenderMetrics_->tilePixels * 1.12)
                                   : .1;
            camera_->setZoom(std::max(fit, camera_->zoom() * multiplier));
        }

        const double newTilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());

        camera_->setPosition(
            worldTileXUnderCursor - screenOffsetX / newTilePixels,
            worldTileYUnderCursor - screenOffsetY / newTilePixels
        );

        clampCameraToWorld();
    }

    void Application::clampCameraToWorld() noexcept
    {
        if (!camera_ || !simulation_ || !renderer_ || !tileRenderMetrics_)
        {
            return;
        }

        if (screen_ == Screen::World && worldRenderer_->globeEnabled)
        {
            // A free sphere has no north/south camera bounds.
            return;
        }
        if (screen_ == Screen::World)
        {
            const auto& g = simulation_->world().grid();
            double x = std::clamp(camera_->tileX(), 0., double(g.width()));
            camera_->setPosition(
                x,
                std::clamp(
                    camera_->tileY(),
                    0.,
                    std::nextafter(double(g.height()), 0.)
                )
            );
            return;
        }
        if (screen_ == Screen::City)
        {
            if (const auto* map =
                    simulation_->settlementMap(activeCitySettlementId_))
            {
                const double fit =
                    std::min(
                        double(renderer_->outputWidth()) / map->grid().width(),
                        double(renderer_->outputHeight()) / map->grid().height()
                    ) /
                    (tileRenderMetrics_->tilePixels * 1.12);
                camera_->setZoom(std::max(camera_->zoom(), fit));
            }
        }
        const double tilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());
        if (tilePixels <= 0)
        {
            return;
        }
        std::int32_t gridWidth = simulation_->world().grid().width();
        std::int32_t gridHeight = simulation_->world().grid().height();

        if (screen_ == Screen::City)
        {
            const SettlementMap* settlementMap =
                simulation_->settlementMap(activeCitySettlementId_);

            if (!settlementMap)
            {
                return;
            }

            gridWidth = settlementMap->grid().width();
            gridHeight = settlementMap->grid().height();
        }

        const double worldWidth = static_cast<double>(gridWidth);
        const double worldHeight = static_cast<double>(gridHeight);

        const double halfVisibleWidth =
            static_cast<double>(renderer_->outputWidth()) / (2.0 * tilePixels);

        const double halfVisibleHeight =
            static_cast<double>(renderer_->outputHeight()) / (2.0 * tilePixels);

        const auto clampAxis = [](double position,
                                  double worldSize,
                                  double halfVisibleSize) noexcept
        {
            if (halfVisibleSize * 2.0 >= worldSize)
            {
                return worldSize * 0.5;
            }

            return std::clamp(
                position,
                halfVisibleSize,
                worldSize - halfVisibleSize
            );
        };

        camera_->setPosition(
            clampAxis(camera_->tileX(), worldWidth, halfVisibleWidth),
            clampAxis(camera_->tileY(), worldHeight, halfVisibleHeight)
        );
    }

    bool Application::activeHudContainsPoint(float x, float y) const noexcept
    {
        if (ledgerPanel_->containsPoint(x, y))
        {
            return true;
        }
        if (debugConsole_->contains(x, y))
        {
            return true;
        }
        if ((screen_ == Screen::City ||
             (screen_ == Screen::World && simulationControlsUnlocked_)) &&
            simulationSpeedControls_->containsInteractivePoint(x, y))
        {
            return true;
        }

        if (screen_ == Screen::City)
        {
            return cityHud_->containsInteractivePoint(x, y) ||
                   employmentPanel_->containsPoint(x, y) ||
                   settlementInspectionPanel_->containsPoint(x, y);
        }

        return WorldMapNavigation::mapBounds(
                   renderer_->outputWidth(),
                   renderer_->outputHeight()
               )
                   .contains(x, y) ||
               WorldMapNavigation::buttonBounds(
                   renderer_->outputWidth(),
                   renderer_->outputHeight()
               )
                   .contains(x, y) ||
               worldHud_->containsInteractivePoint(x, y) ||
               (simulationControlsUnlocked_ &&
                !settlementPlacementController_->isActive() &&
                (cityHud_->containsInteractivePoint(x, y) ||
                 employmentPanel_->containsPoint(x, y)));
    }

    std::optional<SettlementTilePosition> Application::cityTileAtScreen(
        double screenX,
        double screenY
    ) const noexcept
    {
        if (screen_ != Screen::City || !camera_ || !renderer_ ||
            !tileRenderMetrics_)
        {
            return std::nullopt;
        }

        const SettlementMap* settlementMap =
            simulation_->settlementMap(activeCitySettlementId_);

        if (!settlementMap)
        {
            return std::nullopt;
        }

        const double tilePixels =
            tileRenderMetrics_->scaledTilePixels(camera_->zoom());

        if (tilePixels <= 0.0)
        {
            return std::nullopt;
        }

        const double tileX =
            camera_->tileX() +
            (screenX - static_cast<double>(renderer_->outputWidth()) * 0.5) /
                tilePixels;

        const double tileY =
            camera_->tileY() +
            (screenY - static_cast<double>(renderer_->outputHeight()) * 0.5) /
                tilePixels;

        const SettlementTilePosition position{
            static_cast<std::int32_t>(std::floor(tileX)),
            static_cast<std::int32_t>(std::floor(tileY))
        };

        return settlementMap->grid().isValidPosition(position)
                   ? std::optional<SettlementTilePosition>(position)
                   : std::nullopt;
    }

    void Application::handleCameraZoomEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
            (event.key.scancode == SDL_SCANCODE_EQUALS ||
             event.key.scancode == SDL_SCANCODE_KP_PLUS ||
             event.key.scancode == SDL_SCANCODE_MINUS ||
             event.key.scancode == SDL_SCANCODE_KP_MINUS))
        {
            float mouseX = 0.0F;
            float mouseY = 0.0F;
            SDL_GetMouseState(&mouseX, &mouseY);

            const bool zoomingIn = event.key.scancode == SDL_SCANCODE_EQUALS ||
                                   event.key.scancode == SDL_SCANCODE_KP_PLUS;

            applyCameraZoom(
                zoomingIn ? 1.15 : 0.85,
                static_cast<double>(mouseX),
                static_cast<double>(mouseY)
            );
        }

        if (event.type == SDL_EVENT_MOUSE_WHEEL)
        {
            double wheelDelta = static_cast<double>(event.wheel.y);

            if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            {
                wheelDelta = -wheelDelta;
            }

            const double multiplier = wheelDelta > 0.0
                                          ? std::pow(1.15, wheelDelta)
                                          : std::pow(0.85, -wheelDelta);

            applyCameraZoom(
                multiplier,
                static_cast<double>(event.wheel.mouse_x),
                static_cast<double>(event.wheel.mouse_y)
            );
        }
    }

} // namespace Paladin
