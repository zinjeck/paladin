#pragma once

#include "rendering/SceneSpriteLibrary.h"
#include "rendering/WorldObjectPresentation.h"
#include "rendering/WorldPresentation.h"
#include "ui/BitmapFontRenderer.h"
#include "ui/UiTypes.h"
#include "world/World.h"
#include "world/settlements/citizens/SettlementCitizenState.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace Paladin
{
    // One representative for the entire roster. Rendering, unit cards and
    // mouse picking must agree on its authored proportions and ground pivot.
    inline constexpr double WorldArmySpriteScale = .75;

    inline const SceneSprite* worldArmySprite(
        const SceneSpriteLibrary& art, const World& world, const Army& unit)
    {
        const auto* soldier = unit.soldiers().empty() ? nullptr : world.soldier(unit.soldiers().front());
        const auto* source = soldier ? world.settlement(soldier->homeSettlementId()) : nullptr;
        const auto* person = source ? source->simulationState().citizens().citizen(soldier->sourceCitizenId()) : nullptr;
        const std::string sex = person && person->sex == CitizenSex::Female ? "female" : "male";
        return art.find("citizen.militia." + sex + (unit.facingNorth() ? ".back.walk" : ".front.walk"));
    }

    inline double worldArmySpriteScale(double pixels, const SceneSprite* sprite)
    {
        const double height = sprite ? sprite->height : 1.25;
        return worldMovingObjectHeight(pixels) / std::max(1.e-9,pixels*height);
    }

    inline UiRectangle worldArmySpriteBounds(
        double x, double y, double pixels, const SceneSprite* sprite)
    {
        const double scale = worldArmySpriteScale(pixels,sprite);
        const float width = float(pixels * scale * (sprite ? sprite->width : 1.));
        const float height = float(pixels * scale * (sprite ? sprite->height : 1.25));
        return {float(x)-width*float(sprite ? sprite->pivotX : .5),
                float(y)-height*float(sprite ? sprite->pivotY : .9),width,height};
    }

    inline UiRectangle worldArmyCountBounds(double x, double y, std::size_t count, double pixels=0, const SceneSprite* sprite=nullptr)
    {
        const float width = BitmapFontRenderer{}.measureWidth(std::to_string(count),2.F);
        const auto body=pixels>0?worldArmySpriteBounds(x,y,pixels,sprite):UiRectangle{};
        const float top=std::max(float(y)+9.F,body.y+body.height+5.F);
        return {std::round(float(x)-width*.5F)-4.F,std::round(top),width+8.F,20.F};
    }

    inline bool worldArmyHitTest(double px, double py, double x, double y,
                                double pixels, const SceneSprite* sprite, std::size_t count)
    {
        if (!count || worldArmyVisibility(pixels) <= .001F) return false;
        auto body = worldArmySpriteBounds(x,y,pixels,sprite);
        // Allow the common raster's rounding, but don't expand a tiny icon's
        // hitbox to half the screen. The count plate is also a click target.
        const float padding = float(std::max(3.,worldObjectPixelPitch(pixels)));
        body = {body.x-padding,body.y-padding,body.width+2*padding,body.height+2*padding};
        const auto label = worldArmyCountBounds(x,y,count,pixels,sprite);
        const double dx=px-x, dy=py-y;
        return body.contains(float(px),float(py)) ||
               label.contains(float(px),float(py)) || dx*dx+dy*dy <= 14.*14.;
    }
}
