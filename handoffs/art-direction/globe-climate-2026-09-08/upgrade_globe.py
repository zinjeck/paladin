from pathlib import Path
p=Path('C:/Paladin/src/rendering/GlobeRenderer.h')
s=p.read_text()
s=s.replace('#include "rendering/GlobeView.h"','#include "rendering/GlobeView.h"\n#include "rendering/GlobeLighting.h"')
s=s.replace('int w = 0, h = 0;', 'int w = 0, h = 0, left = 0, top = 0, density = 4;')
s=s.replace('        WorldGridRenderer detail_;', '''        struct Source { WorldGrid grid; std::unordered_map<std::string, SceneSprite> art; };
        std::shared_ptr<Source> terrainSource_;
        std::future<Atlas> patchPending_;
        Atlas patchReady_, patchActive_;
        std::unique_ptr<Texture> patchTexture_, patchUpload_;
        int patchUploadRow_ = 0;
        int requestedX_ = -99999, requestedY_ = -99999;
        std::uint64_t patchBuilds_ = 0;''')
s=s.replace('            detail_.reset();','            patchTexture_.reset();\n            patchActive_ = {};')
s=s.replace('            auto b = t.biome;', '            if (t.biome == BiomeType::Polar || t.temperature.value() < .16) return "tundra";\n            auto b = t.biome;')
start=s.index('                    Atlas a;')
end=s.index('                    return a;',start)+len('                    return a;')
body=s[start:end]
body=body.replace('                    const int density =\n                        std::clamp(std::min(4096 / w, 2048 / h), 1, 4);','                    a.left = left; a.top = top; a.density = density;')
body=body.replace('a.w = w * density;', 'a.w = tileWidth * density;').replace('a.h = h * density;','a.h = tileHeight * density;')
body=body.replace('xx = (px + .5) / density,','xx = left + (px + .5) / density,').replace('yy = (py + .5) / density;','yy = top + (py + .5) / density;')
body=body.replace('int(y * density)', 'int((y-top) * density)').replace('(y + height) * density','(y + height-top) * density')
body=body.replace('int(x * density)', 'int((x-left) * density)').replace('(x + width) * density','(x + width-left) * density')
body=body.replace('grid.tile({px / density, py / density})','grid.tile({(left + px / density + w) % w, std::clamp(top + py / density,0,h-1)})')
body=body.replace('((px + .5) / density - x)', '(left + (px + .5) / density - x)').replace('((py + .5) / density - y)', '(top + (py + .5) / density - y)')
body=body.replace('int row = 0; row <= h / 3;', 'int row = std::max(0,(top-8)/3); row <= std::min(h/3,(top+tileHeight+8)/3);')
body=body.replace('int col = 0; col <= w / 4;', 'int col = std::max(0,(left-8)/4); col <= std::min(w/4,(left+tileWidth+8)/4);')
# Separate rocky mountain material from the warmer earth exposed on hills.
body=body.replace('double cumulative = 0, exposure = 0;', 'double cumulative = 0, exposure = 0, rock = 0, ice = 0;')
body=body.replace('                                    exposure +=', '''                                    rock += weight * (n.terrain == TerrainType::Mountain ? 1 : 0);
                                    ice += weight * (n.biome == BiomeType::Polar ? 1 : 0);
                                    exposure +=''')
body=body.replace(': n.biome == BiomeType::Hills ? .42', ': n.biome == BiomeType::Hills ? .68')
needle='                            if (!dry)\n'
body=body.replace(needle,'''                            if (dry && patch < rock * .92)
                            {
                                const double grain = landscapeField(xx*3,yy*3,811);
                                color = grain < .25 ? RenderColor{57,70,88,255} :
                                        grain > .76 ? RenderColor{154,167,175,255} : RenderColor{108,116,122,255};
                            }
                            if (dry && patch < ice)
                            {
                                const double drift = landscapeField(xx*.8, yy*1.5, 519);
                                color = drift < .26 ? RenderColor{126,156,170,255} :
                                        drift > .72 ? RenderColor{244,243,232,255} : RenderColor{215,224,227,255};
                            }
'''+needle)
# Stamp palette remapping belongs to terrain material presentation, retaining
# the authored relief silhouette and transparent cutout.
body=body.replace('double height)\n','double height, bool snowy = false)\n')
body=body.replace('const auto color = (*s.materialPixels)', 'auto color = (*s.materialPixels)')
body=body.replace('                                if (color.alpha)\n', '''                                if (color.alpha && id.find(".ridge.") != std::string::npos)
                                {
                                    const double lum = color.red*.3 + color.green*.5 + color.blue*.2;
                                    const double snowEdge = .25 + .065 * std::sin(sx*.7) + .035 * std::sin(sx*1.9);
                                    if (snowy && double(sy)/s.materialHeight < snowEdge)
                                        color = lum < 90 ? RenderColor{126,156,170,255} : lum < 145 ? RenderColor{175,201,214,255} : RenderColor{244,243,232,255};
                                    else
                                        color = lum < 65 ? RenderColor{53,56,62,255} : lum < 100 ? RenderColor{57,70,88,255} : lum < 135 ? RenderColor{108,116,122,255} : lum < 170 ? RenderColor{154,167,175,255} : RenderColor{215,224,227,255};
                                }
                                if (color.alpha)
''')
body=body.replace('(core ? 3.5 : 2.6) + ((hash >> 18) % 9) / 10.', '(core ? 4.9 : 3.5) + ((hash >> 18) % 9) / 10.,\n                                core || t->temperature.value() < .24')
# Authored hills participate in the same resolution ladder as mountains.
body=body.replace('                    return a;', '''                    if (density >= 8)
                    for (int y = std::max(0,top-4); y < std::min(h,top+tileHeight+4); ++y)
                    for (int x = std::max(0,left-4); x < std::min(w,left+tileWidth+4); ++x)
                    {
                        const auto& t = *grid.tile({x,y});
                        const auto hash = landscapeHash(x,y,171);
                        if (t.biome == BiomeType::Hills && hash % 12 == 0)
                            stamp(1, "world.relief.hill."+std::to_string(1+(hash>>8)%2)+"."+climate(t),
                                  x-.5, y-.4, 3.2, 1.8);
                    }
                    return a;''')
method='''        static Atlas buildAtlas(const WorldGrid& grid,
            const std::unordered_map<std::string, SceneSprite>& art,
            const std::shared_ptr<std::atomic_bool>& cancelled,
            int density, int left, int top, int tileWidth, int tileHeight)
        {
'''+body+ '\n        }\n'
s=s[:start]+'''                    return buildAtlas(source->grid, source->art, cancelled,
                        4, 0, 0, source->grid.width(), source->grid.height());'''+s[end:]
s=s.replace('                [grid = g, art = std::move(art), cancelled = cancelled_]()', '                [source = terrainSource_, cancelled = cancelled_]()')
s=s.replace('            pending_ = std::async(', '''            terrainSource_ = std::make_shared<Source>(Source{g, std::move(art)});
            requestedX_ = requestedY_ = -99999;
            patchTexture_.reset(); patchUpload_.reset(); patchActive_ = {}; patchReady_ = {};
            pending_ = std::async(''')
s=s.replace('    public:\n',method+'\n    public:\n',1)
s=s.replace('for (int i = 0; i < 420;', 'for (int i = 0; i < 1100;')
s=s.replace('hash % 7 ? RenderColor{57, 70, 88, 255}\n                             : RenderColor{154, 167, 175, 255}', 'hash % 7 ? RenderColor{108, 116, 122, 255}\n                             : RenderColor{215, 224, 227, 255}')
light='''                    const auto light = std::uint8_t(
                        255 * (.54 + .46 * std::sqrt(std::max(0., v.p.z)))
                    );'''
s=s.replace(light,'''                    const auto light = globeLight(v.u, v.v, world.time().secondsIntoDay(), v.p.z);''')
s=s.replace('{light, light, light, 255}}','light}')
# Stop independently drawing high quality hills over low quality mountains.
s=s.replace('const bool hill = t.biome == BiomeType::Hills;', 'const bool hill = false;')
p.write_text(s)
