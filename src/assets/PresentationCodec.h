#pragma once
#include "assets/AssetBinary.h"
#include "assets/PresentationData.h"
namespace Paladin
{
    inline AssetBytes encodePresentation(const ObjectPresentation& s)
    {
        AssetWriter w;
        for (auto p : {s.mode, s.floor, s.wall, s.roof, s.sprite, s.decor})
        {
            w.text(p);
        }
        for (double n :
             {s.moduleWidth,
              s.moduleDepth,
              s.height,
              s.thickness,
              s.bodyWidth,
              s.bodyDepth})
        {
            w.real(n);
        }
        for (auto n : {s.frontShade, s.sideShade, s.edgeLight, s.shadowAlpha})
        {
            w.u32(n);
        }
        w.u8(s.outline);
        w.u32(s.fillRgb);
        w.u32(s.frameRgb);
        return w.bytes;
    }
    inline ObjectPresentation decodePresentation(
        std::span<const std::uint8_t> b
    )
    {
        AssetReader r{b};
        ObjectPresentation s;
        for (auto* p :
             {&s.mode, &s.floor, &s.wall, &s.roof, &s.sprite, &s.decor})
        {
            *p = r.text();
        }
        for (auto* p :
             {&s.moduleWidth,
              &s.moduleDepth,
              &s.height,
              &s.thickness,
              &s.bodyWidth,
              &s.bodyDepth})
        {
            *p = r.real();
        }
        for (auto* p :
             {&s.frontShade, &s.sideShade, &s.edgeLight, &s.shadowAlpha})
        {
            *p = int(r.u32());
        }
        s.outline = r.u8() != 0;
        s.fillRgb = r.u32();
        s.frameRgb = r.u32();
        r.end();
        return s;
    }
    inline AssetBytes encodePieces(const std::vector<BuildingPiece>& parts)
    {
        AssetWriter w;
        w.u32(unsigned(parts.size()));
        for (auto& p : parts)
        {
            w.text(p.object);
            w.text(p.sprite);
            w.text(p.state);
            w.real(p.x);
            w.real(p.y);
            w.real(p.depth);
            w.u32(p.choices);
            w.u32(p.choice);
        }
        return w.bytes;
    }
    inline std::vector<BuildingPiece> decodePieces(
        std::span<const std::uint8_t> b
    )
    {
        AssetReader r{b};
        auto n = r.u32();
        if (n > 100000)
        {
            throw std::runtime_error("Too many building pieces");
        }
        std::vector<BuildingPiece> out;
        for (unsigned i = 0; i < n; ++i)
        {
            BuildingPiece p;
            p.object = r.text();
            p.sprite = r.text();
            p.state = r.text();
            p.x = r.real();
            p.y = r.real();
            p.depth = r.real();
            p.choices = r.u32();
            p.choice = r.u32();
            if (!p.choices || p.choice >= p.choices)
            {
                throw std::runtime_error("Invalid piece variants");
            }
            out.push_back(p);
        }
        r.end();
        return out;
    }
    inline AssetBytes encodeLights(const std::vector<BlueprintLight>& lights)
    {
        AssetWriter w;
        w.u32(unsigned(lights.size()));
        for (auto& l : lights)
        {
            w.text(l.object);
            w.real(l.x);
            w.real(l.y);
            w.real(l.radius);
            w.real(l.intensity);
            w.color(l.color);
        }
        return w.bytes;
    }
    inline std::vector<BlueprintLight> decodeLights(
        std::span<const std::uint8_t> b
    )
    {
        AssetReader r{b};
        auto n = r.u32();
        if (n > 100000)
        {
            throw std::runtime_error("Too many lights");
        }
        std::vector<BlueprintLight> out;
        for (unsigned i = 0; i < n; ++i)
        {
            BlueprintLight l;
            l.object = r.text();
            l.x = r.real();
            l.y = r.real();
            l.radius = r.real();
            l.intensity = r.real();
            l.color = r.color();
            if (l.radius <= 0 || l.radius > 16 || l.intensity < 0 ||
                l.intensity > 4)
            {
                throw std::runtime_error("Invalid light");
            }
            out.push_back(l);
        }
        r.end();
        return out;
    }
} // namespace Paladin
