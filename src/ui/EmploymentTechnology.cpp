#include "ui/EmploymentPanel.h"
#include "ui/GrayUiRenderer.h"
#include "world/settlements/citizens/SettlementCitizenState.h"
#include <algorithm>
#include <array>

namespace Paladin
{
    void EmploymentPanel::renderLaws(Renderer& renderer,const GrayUiRenderer& ui,
                                     const SettlementCitizenState& citizens)
    {
        const float left=bounds_.x+16,top=bounds_.y+56;
        const float width=(bounds_.width-46)*.5F,height=bounds_.height-72;
        ui.drawPanel(renderer,{left,top,width,height});
        ui.drawPanel(renderer,{left+width+14,top,width,height});
        float y=top+12;
        const float row=std::min(28.F,(height-138)/10);
        const auto category=[&](const char* title,auto labels,int selected)
        {
            ui.drawLabel(renderer,title,left+12,y,1.5F,{235,196,107,255});
            y+=22;
            int i=0;
            for (const auto* label:labels)
            {
                ui.drawButton(renderer,{left+10,y,width-20,row},label,false,false,i==selected,i==selected);
                y+=row+3; ++i;
            }
            y+=10;
        };
        category("Governance Type",std::array{"Absolute Monarchy","Limited Monarchy",
            "Constitutional Monarchy","Parliamentary Monarchy"},int(citizens.laws().governance));
        category("Citizenship Rights",std::array{"Blood","Accepted Culture","Full Citizenship"},
            int(citizens.laws().citizenship));
        category("Gender Rights",std::array{"Male-Dominated","Equal","Female-Dominated"},
            int(citizens.laws().gender));
        const float right=left+width+14;
        ui.drawLabel(renderer,"Reforms",right+12,top+12,2);
        ui.drawLabel(renderer,"All controlled settlements",right+12,top+40,1);
        ui.drawLabel(renderer,"Work Day",right+12,top+78,1.5F);
        ui.drawLabel(renderer,std::to_string(realmWorkDayHours_)+" hours",right+12,top+106,2);
        for (const int delta:{-1,1})
        {
            const UiRectangle box{right+12+(delta>0?44.F:0.F),top+140,36,28};
            const bool enabled=delta>0?realmWorkDayHours_<14:realmWorkDayHours_>0;
            ui.drawButton(renderer,box,delta>0?">":"<",false,false,false,enabled);
            if (enabled) hits_.push_back({box,"realmWorkDay",{},delta});
        }
        ui.drawLabel(renderer,"Blood: born here to at",right+12,top+196,1);
        ui.drawLabel(renderer,"least one citizen parent.",right+12,top+210,1);
        ui.drawLabel(renderer,"Other law choices are locked.",right+12,top+244,1);
        ui.drawLabel(renderer,"Gender rules govern soldiers",right+12,top+274,1);
        ui.drawLabel(renderer,"and appointed offices.",right+12,top+288,1);
    }
    void EmploymentPanel::renderTechnology(Renderer& renderer,const GrayUiRenderer& ui,
                                           const SettlementCitizenState& citizens)
    {
        constexpr std::array tabs{"Military","Administration","Commerce","Production"};
        const float tabWidth=(bounds_.width-38)/4;
        for (int i=0;i<4;++i)
        {
            const UiRectangle b{bounds_.x+14+i*(tabWidth+3),bounds_.y+50,tabWidth,30};
            ui.drawButton(renderer,b,tabs[i],false,false,i==techTab_,true);
            hits_.push_back({b,"techTab",{},i});
        }
        techCanvas_={bounds_.x+14,bounds_.y+90,bounds_.width-28,bounds_.height-140};
        ui.drawPanel(renderer,techCanvas_);
        const auto previous=renderer.clipRectangle();
        const RenderRectangle clip{techCanvas_.x+2,techCanvas_.y+2,techCanvas_.width-4,techCanvas_.height-4};
        renderer.setClipRectangle(&clip);
        const auto& view=techViews_[techTab_];
        constexpr std::array<std::array<float,2>,9> positions{{{0,0},{-165,132},{165,132},
            {-250,264},{-85,264},{85,264},{250,264},{-165,396},{165,396}}};
        std::array<UiRectangle,9> nodes{};
        for(std::size_t i=0;i<nodes.size();++i)
            nodes[i]={techCanvas_.x+techCanvas_.width*.5F+view.panX+(positions[i][0]-72)*view.zoom,
                techCanvas_.y+20+view.panY+positions[i][1]*view.zoom,144*view.zoom,60*view.zoom};
        constexpr std::array<std::array<int,2>,8> links{{{0,1},{0,2},{1,3},{1,4},{2,5},{2,6},{4,7},{5,8}}};
        for(const auto& link:links)
        {
            const auto& a=nodes[link[0]];const auto& b=nodes[link[1]];
            const float ax=a.x+a.width*.5F,bx=b.x+b.width*.5F;
            const float from=a.y+a.height,to=b.y,middle=(from+to)*.5F;
            const RenderColor ink{99,119,140,255};
            renderer.drawLine(ax,from,ax,middle,ink);
            renderer.drawLine(ax,middle,bx,middle,ink);
            renderer.drawLine(bx,middle,bx,to,ink);
        }
        for(std::size_t i=0;i<nodes.size();++i)
        {
            const auto& box=nodes[i];
            if(box.x+box.width<clip.x || box.x>clip.x+clip.width ||
                box.y+box.height<clip.y || box.y>clip.y+clip.height) continue;
            const bool citizenship=techTab_==1 && i==0;
            ui.drawButton(renderer,box,"",false,false,citizenship && citizens.citizenshipResearched(),citizenship);
            if (citizenship)
            {
                const BitmapFontRenderer font;
                const float scale=std::clamp(view.zoom*1.4F,.7F,2.F);
                ui.drawLabel(renderer,"Citizenship",box.x+(box.width-font.measureWidth("Citizenship",scale))*.5F,box.y+12*view.zoom,scale);
                const auto text=citizens.citizenshipResearched()?"Researched":"Available";
                ui.drawLabel(renderer,text,box.x+(box.width-font.measureWidth(text,view.zoom))*.5F,box.y+38*view.zoom,view.zoom,
                    {235,196,107,255});
                if(!citizens.citizenshipResearched())
                {
                    const float x=std::max(box.x,clip.x),y=std::max(box.y,clip.y);
                    hits_.push_back({{x,y,std::min(box.x+box.width,clip.x+clip.width)-x,
                        std::min(box.y+box.height,clip.y+clip.height)-y},"citizenship"});
                }
            }
        }
        renderer.setClipRectangle(previous?&*previous:nullptr);
        ui.drawLabel(renderer,"Drag tree to pan. Mouse wheel to zoom. Drag title to move window.",
            bounds_.x+16,bounds_.y+bounds_.height-36,1);
        const auto text=techTab_==1?"Citizenship grants local culture and citizenship; retains immigrants' old culture.":
            "Empty branches are reserved for future technologies.";
        const float scale=std::min(1.F,(bounds_.width-32)/(float(std::char_traits<char>::length(text))*6));
        ui.drawLabel(renderer,text,bounds_.x+16,bounds_.y+bounds_.height-20,scale);
    }
}
