// Render the actual overlay menu to an off-screen D3D11 target. No native
// window, Minecraft process, attach helper, input injection or JNI is used.
#include "overlay/overlay.hpp"
#include "common/flat_style.hpp"
#include "common/equipment_icons.hpp"
#include "overlay/menu_motion.hpp"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <crtdbg.h>

using Microsoft::WRL::ComPtr;
using mcov::control_panel::Page;
namespace mcov {
struct OverlayUiCheck {
    static void Draw(Overlay& o,Page page,int theme,bool preserveState) {
        if (!preserveState) { o.menu_.page=page; o.menu_.theme=theme; }
        o.agentConnected_=true;
        o.heartbeatClock_.Observe(++pulse,GetTickCount64());
        o.statusLine_="running";
        o.DrawMenu();
    }
    static void Seed(Overlay& o) {
        o.haveSnapshot_=true;
        o.lastSnapshot_.health=o.lastSnapshot_.maxHealth=20;
        o.lastSnapshot_.fps=144; o.lastSnapshot_.ping=-1;
        o.lastSnapshot_.fovDeg=70;
        CopyField(o.lastSnapshot_.username,"Preview player");
        CopyField(o.lastSnapshot_.dimension,"Overworld");
        CopyField(o.lastSnapshot_.serverAddress,"Local world");
        CopyField(o.lastSnapshot_.gameVersion,"1.21.11");
        CopyField(o.lastSnapshot_.mappingProfile,"intermediary-1.21.11");
    }
    static bool Box(const Overlay& o) { return o.visuals_.boxEsp; }
    static const control_panel::State& Menu(const Overlay& o) { return o.menu_; }
    static void Collapse(Overlay& o,bool value) { o.menu_.navCollapsed=value; }
    static void ReduceMotion(Overlay& o,bool value) { o.menu_.reduceMotion=value; }
    static void Center(Overlay& o) { o.menu_.positioned=false; }
    static float AimFov(const Overlay& o) { return o.combat_.aimFov; }
    static void ResetVisuals(Overlay& o) { o.ResetMenuPage(Page::Visuals); }
    static void AllowCombat(Overlay& o) { o.combat_.aimLock=o.combat_.triggerbot=true; }
    static CombatMenu& CombatEditor(Overlay& o) { return o.combatMenu_; }
    static void ResetCombat(Overlay& o) { o.ResetMenuPage(Page::Combat); }
    static AuraConfig& AuraSettings(Overlay& o) { return o.auraConfig_; }
    static VisualSettings& Visuals(Overlay& o) { return o.visuals_; }
    static void InitIcons(Overlay& o,ID3D11Device* device) {
        if (!o.itemIcons_.Texture() && !o.itemIcons_.Initialize(device))
            throw std::runtime_error("Embedded item atlas initialization failed");
    }
    static void EquipmentPreview(EntitySnapshot& e) {
        e.details.ping=42; e.details.armorValue=20;
        constexpr const char* keys[]={"diamond_helmet","diamond_chestplate","diamond_leggings","diamond_boots","diamond_sword","totem_of_undying"};
        constexpr const char* names[]={"Diamond Helmet","Diamond Chestplate","Diamond Leggings","Diamond Boots","Diamond Sword","Totem of Undying"};
        for (int i=0;i<kSlotCount;++i) {
            auto& item=e.details.equipment[i]; item.present=1; item.count=1;
            CopyField(item.name,names[i]);
            item.iconIndex=equipment_icons::Find(std::string("item.minecraft.")+keys[i]);
            if (i<5) { item.maxDamage=300; item.damage=40+i*40; }
        }
    }
    static void DrawEsp(Overlay& o,const WorldSnapshot& snapshot) {
        o.windowW_=int(ImGui::GetIO().DisplaySize.x); o.windowH_=int(ImGui::GetIO().DisplaySize.y);
        o.agentConnected_=true; o.heartbeatClock_.Observe(++pulse,GetTickCount64());
        o.DrawEsp(snapshot);
    }
    static AuraWorldSample AuraHead(Overlay& o) {
        const auto& target=o.auraRenderer_.Targets()[1];
        if (!target.valid) throw std::runtime_error("Production aura has no current target");
        AuraWorldFog world;
        o.auraRenderer_.BuildWorldFog(target.anchor,o.auraConfig_,world);
        return world.trails[0][0];
    }
    static void DrawAura(Overlay& o,ID3D11Device* device) {
        InitIcons(o,device);
        if (!o.auraBlend_.Callbacks().begin && !o.auraBlend_.Initialize(device))
            throw std::runtime_error("Aura blend preview initialization failed");
        o.windowW_=int(ImGui::GetIO().DisplaySize.x); o.windowH_=int(ImGui::GetIO().DisplaySize.y);
        o.agentConnected_=true; o.heartbeatClock_.Observe(++pulse,GetTickCount64());
        o.auraConfig_.enabled=true;
        auto& s=o.lastSnapshot_;
        s.camX=s.camZ=0; s.camY=1.62; s.yawDeg=s.pitchDeg=0;
        s.fovDeg=70; s.nearPlane=.05f; s.farPlane=1024;
        s.entityCount=1; s.hasTarget=1; s.targetEntityId=42;
        auto& e=s.entities[0]; e={};
        e.entityId=42; e.z=3.5; e.width=.6f; e.height=e.modelHeight=1.8f;
        e.distance=3.5f; e.health=e.maxHealth=20;
        e.flags=kEntityIsPlayer|kEntityIsLiving;
        e.rigKind=kRigHumanoid;
        EquipmentPreview(e);
        CopyField(e.name,"Aura target");
        if (!o.auraPrepared_) { AuraRenderer::Prepare(*ImGui::GetBackgroundDrawList()); o.auraPrepared_=true; }
        o.DrawEsp(s);
    }
    static inline std::uint32_t pulse=0;
};
}
namespace {
void Require(bool good,const char* message) { if (!good) throw std::runtime_error(message); }
void Hr(HRESULT value) { if (FAILED(value)) throw std::runtime_error("Off-screen D3D11/PNG operation failed"); }
void SaveFrame(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& path) {
    const auto size=ImGui::GetIO().DisplaySize;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width=UINT(size.x); desc.Height=UINT(size.y); desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> texture;
    Hr(device->CreateTexture2D(&desc,nullptr,&texture));
    ComPtr<ID3D11RenderTargetView> target;
    Hr(device->CreateRenderTargetView(texture.Get(),nullptr,&target));
    auto* renderTarget=target.Get(); context->OMSetRenderTargets(1,&renderTarget,nullptr);
    const float clear[]={.055f,.06f,.07f,1}; context->ClearRenderTargetView(target.Get(),clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    desc.BindFlags=0; desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Hr(device->CreateTexture2D(&desc,nullptr,&staging));
    context->CopyResource(staging.Get(),texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{}; Hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped));
    ComPtr<IWICImagingFactory> wic;
    Hr(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&wic)));
    ComPtr<IWICStream> stream; Hr(wic->CreateStream(&stream));
    Hr(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder; Hr(wic->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));
    Hr(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame; Hr(encoder->CreateNewFrame(&frame,nullptr));
    Hr(frame->Initialize(nullptr)); Hr(frame->SetSize(desc.Width,desc.Height));
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA; Hr(frame->SetPixelFormat(&format));
    ComPtr<IWICBitmap> bitmap;
    Hr(wic->CreateBitmapFromMemory(desc.Width,desc.Height,GUID_WICPixelFormat32bppRGBA,
        mapped.RowPitch,mapped.RowPitch*desc.Height,static_cast<BYTE*>(mapped.pData),&bitmap));
    ComPtr<IWICFormatConverter> converted; Hr(wic->CreateFormatConverter(&converted));
    Hr(converted->Initialize(bitmap.Get(),format,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    Hr(frame->WriteSource(converted.Get(),nullptr));
    Hr(frame->Commit()); Hr(encoder->Commit());
    context->Unmap(staging.Get(),0);
}
void Draw(mcov::Overlay& overlay,Page page,int theme,ImVec2 size,bool preserveState=false) {
    auto& io=ImGui::GetIO(); io.DisplaySize=size; io.DeltaTime=1.f/60;
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
    auto& ctx=*ImGui::GetCurrentContext();
    const auto colors=ctx.ColorStack.Size,vars=ctx.StyleVarStack.Size,windows=ctx.CurrentWindowStack.Size,fonts=ctx.FontStack.Size;
    const auto before=ImGui::GetStyle();
    mcov::OverlayUiCheck::Draw(overlay,page,theme,preserveState);
    Require(ctx.ColorStack.Size==colors && ctx.StyleVarStack.Size==vars && ctx.CurrentWindowStack.Size==windows && ctx.FontStack.Size==fonts,
            "Menu leaked styles, fonts, child windows or tables");
    const auto& after=ImGui::GetStyle();
    for (int i=0;i<ImGuiCol_COUNT;++i)
        Require(std::memcmp(&before.Colors[i],&after.Colors[i],sizeof(ImVec4))==0,"Menu theme changed the HUD/global palette");
    Require(after.WindowPadding.x==before.WindowPadding.x && after.FrameRounding==before.FrameRounding,
            "Menu theme changed global sizing");
    auto* window=ImGui::FindWindowByName("mcov control panel");
    Require(window && window->Pos.x>=0 && window->Pos.y>=0 &&
            window->Pos.x+window->Size.x<=size.x && window->Pos.y+window->Size.y<=size.y,"Menu escaped the viewport");
    ImGui::Render(); Require(ImGui::GetDrawData()->TotalVtxCount>0,"Empty control panel");
}
ImGuiWindow* Child(const char* name) {
    for (auto* w:ImGui::GetCurrentContext()->Windows)
        if (std::strstr(w->Name,name)) return w;
    throw std::runtime_error("Missing menu child");
}
void EspChecks(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,
               const std::filesystem::path& output) {
    mcov::OverlayUiCheck::InitIcons(overlay,device);
    mcov::WorldSnapshot scene{};
    scene.camY=.95; scene.fovDeg=65; scene.nearPlane=.05f; scene.farPlane=1024;
    scene.ping=0; scene.entityCount=1; scene.hasTarget=1; scene.targetEntityId=42;
    auto& entity=scene.entities[0];
    entity.entityId=42; entity.z=3.3; entity.width=.6f; entity.height=entity.modelHeight=1.8f;
    entity.distance=3.3f; entity.health=entity.maxHealth=20;
    entity.flags=mcov::kEntityIsPlayer|mcov::kEntityIsLiving;
    entity.rigKind=mcov::kRigHumanoid;
    mcov::OverlayUiCheck::EquipmentPreview(entity);
    mcov::CopyField(entity.name,"test_entity");
    auto& config=mcov::OverlayUiCheck::Visuals(overlay);
    config=mcov::VisualSettings{};
    const auto frame=[&](ImVec2 viewport) {
        auto& io=ImGui::GetIO(); io.DisplaySize=viewport; io.DeltaTime=1.f/60;
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        auto& draw=*ImGui::GetBackgroundDrawList();
        const int clips=draw._ClipRectStack.Size;
        mcov::OverlayUiCheck::DrawEsp(overlay,scene);
        Require(draw._ClipRectStack.Size==clips,"ESP leaked a clipping rectangle");
        for (const auto& vertex:draw.VtxBuffer)
            Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"ESP generated invalid vertices");
        const int count=draw.VtxBuffer.Size;
        ImGui::Render(); return count;
    };
    Require(frame({585,745})>0,"Production ESP preview was empty");
    SaveFrame(device,context,output/"esp-reference.png");
    entity.health=8;
    Require(frame({1280,800})>0,"Partially depleted entity lost its ESP");
    SaveFrame(device,context,output/"esp-health.png");
    // Namen ohne Nullterminator, fehlende Messwerte und Randpositionen sind echte IPC-Grenzfälle.
    std::memset(entity.name,'W',sizeof(entity.name));
    scene.ping=-1; entity.details.ping=-1; entity.x=2.2; entity.health=0;
    Require(frame({480,360})>0,"Clipped ESP with a long name was rejected");
    SaveFrame(device,context,output/"esp-edge.png");
    entity.distance=std::numeric_limits<float>::quiet_NaN();
    Require(frame({585,745})==0,"Invalid entity distance reached ESP or target info");
    entity.distance=3.3f; entity.x=0; entity.flags|=mcov::kEntityIsSelf;
    Require(frame({585,745})==0,"Self entity leaked into ESP or target info");
    entity.flags&=~mcov::kEntityIsSelf; config.showPlayers=false;
    Require(frame({585,745})==0,"Player filter did not hide the target info");
    config=mcov::VisualSettings{};
    config.boxEsp=config.skeleton=config.tracers=config.nameTags=config.healthBars=config.distanceText=config.targetInfo=false;
    Require(frame({585,745})==0,"Disabling every ESP feature still drew target info");
    config=mcov::VisualSettings{};
}
}
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0,_WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    try {
        const auto output=std::filesystem::absolute(argc>1 ? argv[1] : "ui-preview");
        std::filesystem::create_directories(output);
        Hr(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        ImGui::CreateContext(); auto& io=ImGui::GetIO(); io.IniFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        Require(mcov::style::LoadFonts(),"Embedded fonts failed");
        Require(mcov::control_panel::motion::EaseOut(0)==0 && mcov::control_panel::motion::EaseOut(1)==1 &&
                mcov::control_panel::motion::EaseOut(.2f)>.5f,"Feedback curve does not respond promptly");
        mcov::style::ApplyFlatTheme(ImGui::GetStyle());
        Require(ImGui_ImplDX11_Init(device.Get(),context.Get()),"Off-screen ImGui renderer failed");
        mcov::Overlay overlay; mcov::OverlayUiCheck::Seed(overlay);
        const Page pages[]={Page::Combat,Page::Visuals,Page::Blocks,Page::Hud,Page::Stats,Page::Settings};
        for (int theme=0;theme<4;++theme) {
            for (Page page:pages) {
                Draw(overlay,page,theme,ImVec2(1280,800));
                Draw(overlay,page,theme,ImVec2(1280,800));
                if (page==Page::Visuals) {
                    for (int i=0;i<12;++i) Draw(overlay,page,theme,ImVec2(1280,800));
                    SaveFrame(device.Get(),context.Get(),output/("theme-"+std::to_string(theme)+".png"));
                }
            }
        }
        Require(mcov::style::MenuIconFont()->FindGlyphNoFallback(0xf54c),"Embedded skull icon is missing");
        Require(mcov::style::MenuFont()->FontSize==14 && io.FontDefault->FontSize==17,
                "Larger menu font changed the HUD font");
        mcov::OverlayUiCheck::AllowCombat(overlay);
        mcov::OverlayUiCheck::Center(overlay);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        auto* panel=ImGui::FindWindowByName("mcov control panel");
        Require(panel->Size.x<=900 && panel->Size.y<=510,"Menu footprint was not reduced");
        for (int i=0;i<12;++i) Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        SaveFrame(device.Get(),context.Get(),output/"combat.png");
        Draw(overlay,Page::Combat,0,ImVec2(752,427));
        Draw(overlay,Page::Combat,0,ImVec2(752,427));
        SaveFrame(device.Get(),context.Get(),output/"reference-size.png");
        // Alle Combat-Bereiche verwenden echte Stores und dieselbe begrenzte Menüfläche.
        auto& editor=mcov::OverlayUiCheck::CombatEditor(overlay);
        const mcov::CombatMenu::Section sections[]={mcov::CombatMenu::Section::KillAura,mcov::CombatMenu::Section::Targets,mcov::CombatMenu::Section::Rotation,
            mcov::CombatMenu::Section::Attacks,mcov::CombatMenu::Section::Sync,mcov::CombatMenu::Section::Friends,mcov::CombatMenu::Section::Debug};
        constexpr const char* sectionFiles[]={"combat-kill-aura.png","combat-targets.png","combat-rotation.png","combat-attacks.png","combat-sync.png","combat-friends.png","combat-debug.png"};
        for(int theme=0;theme<4;++theme) for(int i=0;i<IM_ARRAYSIZE(sections);++i) {
            editor.Select(sections[i]);
            for(int pass=0;pass<3;++pass) Draw(overlay,Page::Combat,theme,ImVec2(1280,800));
            Require(editor.Current()==sections[i],"Combat subtab selection was lost");
            Require(mcov::combat::Validate(editor.Store().Read().values),"Rendering Combat controls invalidated config");
            if(theme==0) SaveFrame(device.Get(),context.Get(),output/sectionFiles[i]);
            Draw(overlay,Page::Combat,theme,ImVec2(752,427));
        }
        editor.Select(mcov::CombatMenu::Section::KillAura);
        for(int pass=0;pass<3;++pass) Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        auto* auraContent=Child("/content_");
        auto* auraTable=ImGui::GetCurrentContext()->Tables.GetByKey(auraContent->GetID("kill_aura_controls"));
        Require(auraTable,"Kill Aura controls missing");
        auto clickAura=[&](ImVec2 position) {
            io.AddMousePosEvent(position.x,position.y); io.AddMouseButtonEvent(0,true); Draw(overlay,Page::Combat,0,ImVec2(1280,800));
            io.AddMouseButtonEvent(0,false); Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        };
        clickAura(ImVec2(auraTable->Columns[0].MinX+40,auraTable->OuterRect.Min.y+18));
        const auto live=editor.Store().Read().values;
        Require(live.enabled && live.attack.enabled && live.rotation.enabled && live.sync.enabled,"Kill Aura switch did not activate the live core");
        auraTable=ImGui::GetCurrentContext()->Tables.GetByKey(auraContent->GetID("kill_aura_controls"));
        clickAura(ImVec2(auraTable->OuterRect.Min.x+65,auraTable->OuterRect.Max.y+12));
        Require(editor.Store().Read().values.target.coneDegrees==180 && editor.Store().Read().values.target.range==3 && editor.Store().Read().values.enabled,"Kill Aura preset lost activation or settings");
        mcov::OverlayUiCheck::ResetCombat(overlay);
        const auto uuid=mcov::CombatMenu::ParseUuid("12345678-1234-5678-90ab-cdef12345678");
        Require(uuid && uuid->high==0x1234567812345678 && uuid->low==0x90abcdef12345678,"Player UUID was parsed incorrectly");
        Require(!mcov::CombatMenu::ParseUuid("12345678_1234-5678-90ab-cdef12345678") &&
                !mcov::CombatMenu::ParseUuid("00000000000000000000000000000000") &&
                !mcov::CombatMenu::ParseUuid(nullptr),"Invalid player UUID was accepted");
        editor.Select(mcov::CombatMenu::Section::Targets);
        for(int pass=0;pass<3;++pass) Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        auto* combatContent=Child("/content_");
        auto* outer=ImGui::GetCurrentContext()->Tables.GetByKey(combatContent->GetID("combat_assist_settings"));
        Require(outer,"Combat settings table missing");
        auto* targets=ImGui::GetCurrentContext()->Tables.GetByKey(ImHashStr("target_selection",0,outer->ID));
        Require(targets,"Target selector table missing");
        const ImVec2 targetToggle(targets->Columns[0].MinX+38,targets->OuterRect.Min.y+18);
        io.AddMousePosEvent(targetToggle.x,targetToggle.y); io.AddMouseButtonEvent(0,true);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        io.AddMouseButtonEvent(0,false); Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        Require(!editor.Store().Read().values.target.enabled,"Target toggle was not published to ConfigStore");
        targets=ImGui::GetCurrentContext()->Tables.GetByKey(ImHashStr("target_selection",0,outer->ID));
        const ImVec2 doubleSlider(targets->Columns[1].MaxX-10,targets->OuterRect.Min.y+5*35.25f+17);
        io.AddMousePosEvent(doubleSlider.x,doubleSlider.y); io.AddMouseButtonEvent(0,true);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        io.AddMouseButtonEvent(0,false); Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        Require(editor.Store().Read().values.target.coneDegrees>60,"Double slider did not publish the assist FOV");
        Require(editor.Relations().Set(*uuid,mcov::combat::Relation::Friend),"UUID friend insertion failed");
        mcov::OverlayUiCheck::ResetCombat(overlay);
        Require(editor.Store().Read().values.target.enabled && editor.Store().Read().values.target.coneDegrees==20 &&
                mcov::combat::Lookup(editor.Relations().Read(),*uuid)==mcov::combat::Relation::Neutral,"Combat reset left config or relations behind");
        for(int pass=0;pass<3;++pass) Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        mcov::OverlayUiCheck::AllowCombat(overlay);
        // Click the first real Visuals row label; the backend setting must change.
        Draw(overlay,Page::Visuals,0,ImVec2(1280,800));
        auto* content=Child("/content_");
        const ImVec2 click(content->DC.CursorStartPos.x+38,content->DC.CursorStartPos.y+18);
        io.AddMousePosEvent(click.x,click.y); io.AddMouseButtonEvent(0,true);
        Draw(overlay,Page::Visuals,0,ImVec2(1280,800));
        io.AddMouseButtonEvent(0,false); Draw(overlay,Page::Visuals,0,ImVec2(1280,800));
        Require(!mcov::OverlayUiCheck::Box(overlay),"Row label was not wired to the real Box ESP setting");
        mcov::OverlayUiCheck::ResetVisuals(overlay);
        Require(mcov::OverlayUiCheck::Box(overlay),"Reset did not restore real settings");
        // Exercise the navigation through ImGui input; retain state across frames.
        auto clickMenu=[&](ImVec2 at) {
            io.AddMousePosEvent(at.x,at.y); io.AddMouseButtonEvent(0,true);
            Draw(overlay,Page::Visuals,0,ImVec2(1280,800),true);
            io.AddMouseButtonEvent(0,false);
            Draw(overlay,Page::Visuals,0,ImVec2(1280,800),true);
            Draw(overlay,Page::Visuals,0,ImVec2(1280,800),true);
        };
        auto* nav=Child("/navigation_");
        clickMenu(ImVec2(nav->DC.CursorStartPos.x+nav->WorkRect.GetWidth()*.5f,nav->DC.CursorStartPos.y+38));
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Combat,"Navigation did not select Combat");
        nav=Child("/navigation_");
        clickMenu(ImVec2(nav->DC.CursorStartPos.x+nav->WorkRect.GetWidth()*.5f,nav->DC.CursorStartPos.y+126));
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Visuals,"Navigation did not select Visuals");
        auto* tools=Child("/tools_");
        clickMenu(ImVec2(tools->DC.CursorStartPos.x+21,tools->DC.CursorStartPos.y+22));
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Stats,"Player icon did not select statistics");
        tools=Child("/tools_");
        clickMenu(ImVec2(tools->DC.CursorStartPos.x+21,tools->DC.CursorStartPos.y+125));
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Settings,"Gear icon did not select settings");
        mcov::OverlayUiCheck::Collapse(overlay,true);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        nav=Child("/navigation_");
        Require(nav->Size.x==35,"Collapsed navigation did not resize");
        SaveFrame(device.Get(),context.Get(),output/"collapsed.png");
        mcov::OverlayUiCheck::Collapse(overlay,false);
        // Drag the custom thin slider. Its numeric state is the combat module's
        // real value, and remains clamped to the module's supported range.
        Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        content=Child("/content_");
        const ImVec2 slider(content->DC.CursorStartPos.x+content->Size.x*.5f-24,
                            content->DC.CursorStartPos.y+editor.NavigationHeight()+mcov::style::MenuFont()->FontSize+12.5f+2*35.25f+17);
        io.AddMousePosEvent(slider.x,slider.y); io.AddMouseButtonEvent(0,true);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        io.AddMouseButtonEvent(0,false); Draw(overlay,Page::Combat,0,ImVec2(1280,800));
        Require(mcov::OverlayUiCheck::AimFov(overlay)>60 && mcov::OverlayUiCheck::AimFov(overlay)<=90,
                "Thin slider did not update the real aim lock cone");
        // Drag the empty top strip; both floating panels move as a single menu.
        panel=ImGui::FindWindowByName("mcov control panel");
        const ImVec2 original=panel->Pos;
        const ImVec2 grip(original.x+panel->Size.x*.4f,original.y+10);
        io.AddMousePosEvent(grip.x,grip.y); io.AddMouseButtonEvent(0,true);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        Require(mcov::OverlayUiCheck::Menu(overlay).dragging,"Drag strip did not capture the mouse");
        io.AddMousePosEvent(grip.x+64,grip.y+45);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        io.AddMouseButtonEvent(0,false);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        panel=ImGui::FindWindowByName("mcov control panel");
        const ImVec2 moved=panel->Pos;
        Require(moved.x==original.x+64 && moved.y==original.y+45 && !mcov::OverlayUiCheck::Menu(overlay).dragging,
                "Menu drag lagged, changed its offset or failed to release");
        for (int i=0;i<12;++i) Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        Require(panel->Pos.x==moved.x && panel->Pos.y==moved.y,"Menu position snapped back after dragging");
        SaveFrame(device.Get(),context.Get(),output/"dragged.png");
        const ImVec2 edgeGrip(moved.x+panel->Size.x*.4f,moved.y+10);
        io.AddMousePosEvent(edgeGrip.x,edgeGrip.y); io.AddMouseButtonEvent(0,true);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        io.AddMousePosEvent(-200,-200);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        io.AddMouseButtonEvent(0,false);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        Require(panel->Pos.x==0 && panel->Pos.y==0,"Drag escaped the viewport");
        mcov::OverlayUiCheck::Center(overlay);
        Draw(overlay,Page::Combat,0,ImVec2(1280,800),true);
        mcov::OverlayUiCheck::ReduceMotion(overlay,true);
        nav=Child("/navigation_");
        clickMenu(ImVec2(nav->DC.CursorStartPos.x+nav->WorkRect.GetWidth()*.5f,nav->DC.CursorStartPos.y+126));
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Visuals &&
                mcov::OverlayUiCheck::Menu(overlay).pageAlpha==1,"Reduced motion delayed navigation");
        mcov::OverlayUiCheck::ReduceMotion(overlay,false);
        for (auto size:{ImVec2(720,560),ImVec2(480,360)})
            for (Page page:pages) Draw(overlay,page,2,size);
        for (int i=0;i<12;++i) Draw(overlay,Page::Settings,2,ImVec2(480,360));
        SaveFrame(device.Get(),context.Get(),output/"compact.png");
        io.AddMousePosEvent(-100,-100);
        EspChecks(overlay,device.Get(),context.Get(),output);
        auto auraFrame=[&] {
            io.DisplaySize={1280,800}; io.DeltaTime=1.f/60;
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            mcov::OverlayUiCheck::DrawAura(overlay,device.Get()); ImGui::Render();
        };
        auto& aura=mcov::OverlayUiCheck::AuraSettings(overlay);
        aura.pulseAmplitude=0;
        mcov::OverlayUiCheck::ReduceMotion(overlay,true);
        for (int i=0;i<16;++i) auraFrame();
        const auto initialHead=mcov::OverlayUiCheck::AuraHead(overlay);
        for (int i=0;i<30;++i) auraFrame();
        const auto rotatedHead=mcov::OverlayUiCheck::AuraHead(overlay);
        const auto distance=[](const auto& a,const auto& b) {
            const double dx=a.x-b.x,dy=a.y-b.y,dz=a.z-b.z;
            return std::sqrt(dx*dx+dy*dy+dz*dz);
        };
        Require(distance(initialHead,rotatedHead)>.3,"Menu/Windows motion settings blocked enabled aura rotation");
        aura.animate=false;
        for (int i=0;i<30;++i) auraFrame();
        const auto pausedHead=mcov::OverlayUiCheck::AuraHead(overlay);
        Require(distance(rotatedHead,pausedHead)<.00001,"Production aura ignored its animation toggle");
        aura.animate=true;
        for (int i=0;i<30;++i) auraFrame();
        Require(distance(pausedHead,mcov::OverlayUiCheck::AuraHead(overlay))>.3,
                "Production aura did not resume after enabling animation");
        mcov::OverlayUiCheck::ReduceMotion(overlay,false);
        aura.pulseAmplitude=.035f;
        for (int i=0;i<16;++i) auraFrame();
        SaveFrame(device.Get(),context.Get(),output/"aura.png");
        ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext(); CoUninitialize();
        std::cout<<"PASS control panel: compact geometry, larger scoped fonts, drag capture/release and bounds, reduced menu motion, independent aura rotation/pause/resume, embedded icons, 6 pages, 4 themes, settings binding, navigation and small viewports\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
