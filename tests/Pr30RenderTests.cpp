#include "TestFramework.h"
#include "platform/Window.h"
#include "rendering/Renderer.h"
#include "rendering/SceneSpriteLibrary.h"
#include "rendering/SelectionOutline.h"
#include "rendering/WorldMapNavigation.h"
#include "rendering/WorldPixelGrid.h"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <iostream>
#include <memory>

namespace
{
    using namespace Paladin;
    using Surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    std::vector<std::uint32_t> capture(SDL_Window* window, const std::string& name)
    {
        Surface raw(SDL_RenderReadPixels(SDL_GetRenderer(window), nullptr), SDL_DestroySurface);
        PALADIN_CHECK(raw);
        Surface rgba(SDL_ConvertSurface(raw.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
        PALADIN_CHECK(rgba);
        if (const auto* root = SDL_getenv("PALADIN_SMOKE_SCREENSHOTS"))
        {
            std::filesystem::create_directories(root);
            PALADIN_CHECK(IMG_SavePNG(rgba.get(), (std::filesystem::path(root)/name).string().c_str()));
        }
        std::vector<std::uint32_t> result;
        for (int y=0;y<rgba->h;++y)
        {
            const auto* row=reinterpret_cast<const std::uint32_t*>(static_cast<const Uint8*>(rgba->pixels)+y*rgba->pitch);
            result.insert(result.end(),row,row+rgba->w);
        }
        return result;
    }
    void outlines()
    {
        for (double pitch : {1.,1.01,1.5,2.,3.3,4.,8.,12.})
            for (int phase=0;phase<100;++phase)
                for (float size : {.2F,1.F,4.F,25.F,109.F})
                {
                    const auto edges=selectionBorder({float(phase*.37-8),float(phase*.19-5),size,size*2},pitch);
                    for (const auto& e:edges)
                    {
                        PALADIN_CHECK(e.width>=pitch-1e-4 && e.height>=pitch-1e-4);
                        PALADIN_CHECK(std::abs(e.x/pitch-std::round(e.x/pitch))<1e-4);
                        PALADIN_CHECK(std::abs(e.y/pitch-std::round(e.y/pitch))<1e-4);
                    }
                    PALADIN_CHECK(std::abs(edges[0].x+edges[0].width-(edges[3].x+edges[3].width))<1e-4);
                    PALADIN_CHECK(std::abs(edges[1].y+edges[1].height-(edges[2].y+edges[2].height))<1e-4);
                }
        for (auto [w,h]:{std::pair{640,480},{1024,768},{1600,900},{2560,1440}})
        {
            const auto map=WorldMapNavigation::mapBounds(w,h), buttons=WorldMapNavigation::buttonBounds(w,h);
            PALADIN_CHECK(buttons.x==map.x && buttons.y+buttons.height<map.y && buttons.x>w*.5);
        }
        std::cout<<"PR30: 4000 border phases and four bottom-right minimap layouts passed\n";
    }
    void silhouettes(Renderer& renderer, SDL_Window* window, SceneSpriteLibrary& art)
    {
        int comparisons=0;
        for (const char* role : {"citizen","militia","logger"})
            for (const char* sex : {"male","female"})
                for (double pixels : {6.,10.,16.,24.,63.5,128.})
                {
                    const auto* sprite=art.find(std::string("citizen.")+role+"."+sex+".front.walk");
                    PALADIN_CHECK(sprite && !sprite->selectionMask.alpha.empty());
                    for (int pose=0;pose<4;++pose)
                    {
                        SceneProjection projection{10.375,10.4375,pixels,640,480};
                        SceneDrawItem item;
                        item.bounds=projection.bounds({10.5,10.5,sprite->elevation,sprite->width,sprite->height,sprite->pivotX,sprite->pivotY});
                        item.texture=sprite->texture.get(); item.atlasFrame=art.frame(*sprite,false);
                        item.atlasFrame.x=pose*item.atlasFrame.width;
                        const auto draw=[&](bool selected)
                        {
                            renderer.beginFrame();
                            {
                                WorldPixelScene scene(renderer,pixels);
                                renderer.fillRectangle(0,0,640,480,{8,15,27,255});
                                SceneDrawQueue queue; queue.submit(item); queue.render(renderer);
                                if(selected) PALADIN_CHECK(art.renderSelection(renderer,item));
                            }
                            auto image=capture(window,std::string("pr30-silhouette-")+role+"-"+sex+"-"+std::to_string(int(pixels))+"-"+std::to_string(pose)+(selected?"-selected.png":"-plain.png"));
                            renderer.endFrame(); return image;
                        };
                        const auto plain=draw(false), selected=draw(true);
                        std::size_t changed=0;
                        const auto background=plain[0];
                        for(std::size_t i=0;i<plain.size();++i)
                        {
                            // No tinted box or yellow silhouette interior. Every
                            // changed pixel must lie outside the visible sprite.
                            if(plain[i]!=selected[i]) { ++changed; PALADIN_CHECK(plain[i]==background); }
                        }
                        PALADIN_CHECK(changed>0); ++comparisons;
                    }
                }
        std::cout<<"PR30: "<<comparisons<<" animated citizen silhouette comparisons passed\n";
    }
}
int main()
{
    if(!SDL_Init(SDL_INIT_VIDEO)) return 1;
    int result=0;
    try
    {
        outlines();
        Window window("Paladin PR30 regressions",640,480);
        PALADIN_CHECK(window.isValid()); SDL_HideWindow(window.nativeHandle());
        Renderer renderer(window.nativeHandle()); PALADIN_CHECK(renderer.isValid());
        SceneSpriteLibrary art; art.load(renderer,std::string(PALADIN_TEST_SOURCE_ROOT)+"/assets/sprites");
        silhouettes(renderer,window.nativeHandle(),art);
    }
    catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; result=1; }
    SDL_Quit(); return result;
}
