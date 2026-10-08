// Render the actual overlay menu to an off-screen D3D11 target. No native
// window, Minecraft process, attach helper, input injection or JNI is used.
#include "overlay/overlay.hpp"
#include "common/flat_style.hpp"
#include "common/equipment_icons.hpp"
#include "overlay/menu_motion.hpp"
#include "injector/launcher_ui.hpp"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
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
    static void CloseDetails(Overlay& o) { o.menu_.settingsOpen=false; }
    static int Modules(const Overlay& o,Page page,std::span<control_panel::ModuleView> out) { return o.ReadModules(page,out); }
    static void ReduceMotion(Overlay& o,bool value) { o.menu_.reduceMotion=value; }
    static void Center(Overlay& o) { o.menu_.positioned=false; }
    static float AimFov(const Overlay& o) { return o.combat_.aimFov; }
    static void ResetVisuals(Overlay& o) { o.ResetMenuPage(Page::Visuals); }
    static void AllowCombat(Overlay& o) { o.combat_.aimLock=o.combat_.triggerbot=true; }
    static CombatMenu& CombatEditor(Overlay& o) { return o.combatMenu_; }
    static void ResetCombat(Overlay& o) { o.ResetMenuPage(Page::Combat); }
    static AuraConfig& AuraSettings(Overlay& o) { return o.auraConfig_; }
    static TargetCubeConfig& CubeSettings(Overlay& o) { return o.targetCubeConfig_; }
    static void MarkerDetails(Overlay& o,bool open) { o.markerDetailsOpen_=open; }
    static HudSettings& Hud(Overlay& o) { return o.hud_; }
    static OverlaySettings& Settings(Overlay& o) { return o.settings_; }
    static WorldVisualsConfig& WorldSettings(Overlay& o) { return o.worldVisualsConfig_; }
    static LocalHatConfig& HatSettings(Overlay& o) { return o.localHatConfig_; }
    static rockstar::Config& Rockstar(Overlay& o) { return o.rockstarConfig_; }
    static rockstar::Pack& Pack(Overlay& o) { return o.rockstarPack_; }
    static void Snapshot(Overlay& o,const WorldSnapshot& s) { o.lastSnapshot_=s; o.haveSnapshot_=true; }
    static bool Module(Overlay& o,keys::Action a) { return o.ModuleEnabled(a); }
    static void SetModule(Overlay& o,keys::Action a,bool on) { o.SetModuleEnabled(a,on); }
    static void Panic(Overlay& o) { o.Panic(); }
    static void UpdateWorld(Overlay& o,const WorldSnapshot& s,double now) {
        o.worldVisuals_.Update(s,now,o.worldVisualsConfig_,s.hasTarget?s.targetEntityId:0);
    }
    static ChunkFinderSettings& Chunk(Overlay& o) { return o.chunkFinder_; }
    static items::MacroConfig& Crystal(Overlay& o) { return o.crystalMacro_; }
    static items::MacroConfig& Anchor(Overlay& o) { return o.anchorMacro_; }
    static items::Request ItemRequest(const Overlay& o,bool allowed) { return o.BuildItemRequest(1000,allowed,false); }
    static void ChunkTab(Overlay& o) { o.blocksSection_=3; }
    static void ItemStatus(Overlay& o) { o.itemReport_.totem=items::Status::HoldingTotem; o.itemReport_.crystal=items::Status::WaitingBind; o.itemReport_.anchor=items::Status::WaitingBind; }
    static int DrawChunk(Overlay& o,const WorldSnapshot& s) {
        const auto size=ImGui::GetIO().DisplaySize;
        const CameraProjection projection(s,size.x,size.y);
        auto* draw=ImGui::GetBackgroundDrawList(); const int before=draw->VtxBuffer.Size;
        o.DrawChunkFinder(s,projection); return draw->VtxBuffer.Size-before;
    }
    static keys::Bindings& Binds(Overlay& o) { return o.bindings_; }
    static int Capture(const Overlay& o) { return o.captureAction_; }
    static void Capture(Overlay& o,keys::Action a) { o.captureAction_=keys::Index(a); o.captureArmed_=true; o.menuVisible_=true; o.capturePrevious_={}; }
    static void Input(Overlay& o,const keys::Input& input,bool menu=false) { o.menuVisible_=menu; o.ProcessKeyInput(input,true); }
    static void PrefPath(Overlay& o,const std::filesystem::path& path) { o.preferencesPath_=path; }
    static void Save(Overlay& o) { o.SavePreferences(); }
    static void Load(Overlay& o) { o.LoadPreferences(); }
    static void DrawHud(Overlay& o,const WorldSnapshot& s,bool editing) {
        o.windowW_=int(ImGui::GetIO().DisplaySize.x); o.windowH_=int(ImGui::GetIO().DisplaySize.y);
        o.hudEditing_=editing; o.DrawHud(s); if(editing) o.DrawHudEditor();
    }
    static const auto& HudRects(const Overlay& o) { return o.hudRects_; }
    static void CubeReport(Overlay& o,const EntitySnapshot& e,CombatBridgeStatus status=CombatBridgeStatus::Active,std::uint64_t age=0) {
        auto config=combat::MakeKillAuraConfig({}); config.enabled=true; o.combatMenu_.Store().Publish(config);
        o.combatReport_={}; o.combatReport_.sampledAtMs=GetTickCount64()-age;
        o.combatReport_.status=status; o.combatReport_.debug.enabled=true;
        combat::Target target; target.id=e.entityId; target.uuid=e.uuid; o.combatReport_.debug.target=target;
    }
    static void StopCube(Overlay& o) { o.combatMenu_.Store().Publish(combat::MakeKillAuraConfig({})); o.combatReport_={}; }
    static double CubeAngle(const Overlay& o) { return o.targetCube_.Angle(); }
    static VisualSettings& Visuals(Overlay& o) { return o.visuals_; }
    static void InitIcons(Overlay& o,ID3D11Device* device) {
        if (!o.itemIcons_.Texture() && !o.itemIcons_.Initialize(device))
            throw std::runtime_error("Embedded item atlas initialization failed");
    }
    static void InitGlow(Overlay& o,ID3D11Device* device) {
        if (!o.auraBlend_.Callbacks().begin && !o.auraBlend_.Initialize(device))
            throw std::runtime_error("Friend hat glow initialization failed");
        FriendHatRenderer::Prepare(*ImGui::GetBackgroundDrawList());
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
mcov::LiquidGlass* previewGlass=nullptr;
ComPtr<ID3D11Texture2D> previewScene;
ComPtr<ID3D11ShaderResourceView> previewSceneImage;
void Require(bool good,const char* message) { if (!good) throw std::runtime_error(message); }
void Hr(HRESULT value) { if (FAILED(value)) throw std::runtime_error("Off-screen D3D11/PNG operation failed"); }
void SaveFrame(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& path) {
    if(previewGlass) previewGlass->RenderTexture();
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
    if(previewGlass) previewGlass->BeginFrame(nullptr,size,true,false,previewScene.Get());
    if(previewSceneImage) ImGui::GetBackgroundDrawList()->AddImage(
        static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(previewSceneImage.Get())),{0,0},size);
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
    for(auto* w:ImGui::GetCurrentContext()->Windows) {
        const auto* match=std::strstr(w->Name,name);
        if(match&&!std::strchr(match+1,'/')) return w;
    }
    throw std::runtime_error("Missing menu child");
}
// Echte Rechtsklicks erreichen auch Module unterhalb der sichtbaren Grid-Zeilen.
ImGuiTabBar* ModuleTabs(Page page) {
    auto* root=ImGui::FindWindowByName("mcov control panel");
    const int index=int(page);
    const auto scope=ImHashData(&index,sizeof(index),root->IDStack.back());
    return ImGui::GetCurrentContext()->TabBars.GetByKey(ImHashStr("module_tabs",0,scope));
}
void SelectSubTab(mcov::Overlay& overlay,Page page,const char* id,int index,int count,ImVec2 viewport={1280,800}) {
    (void)id;
    auto frame=[&] { Draw(overlay,page,0,viewport); };
    ImGui::GetIO().AddMousePosEvent(-100,-100); frame(); frame(); frame();
    auto* bar=ModuleTabs(page); Require(bar&&bar->Tabs.Size==count,"Module tabs are incomplete");
    const auto& tab=bar->Tabs[index];
    bar->ScrollingAnim=bar->ScrollingTarget=std::max(0.f,tab.Offset+tab.Width*.5f-bar->BarRect.GetWidth()*.5f);
    frame(); frame();
    const ImVec2 p{bar->BarRect.Min.x+tab.Offset-bar->ScrollingAnim+tab.Width*.5f,bar->BarRect.GetCenter().y};
    auto& io=ImGui::GetIO(); io.AddMousePosEvent(p.x,p.y); frame();
    io.AddMouseButtonEvent(0,true); frame(); io.AddMouseButtonEvent(0,false); frame(); frame();
    Require(mcov::OverlayUiCheck::Menu(overlay).settingsPage==page&&mcov::OverlayUiCheck::Menu(overlay).settingsSection==index,"Module tab did not select its actual controls");
    io.AddMousePosEvent(-100,-100); frame();
}
void ModuleTileChecks(mcov::Overlay& overlay) {
    auto& io=ImGui::GetIO();
    for(Page page:{Page::Combat,Page::Visuals,Page::Blocks,Page::Hud,Page::Misc}) {
        std::array<mcov::control_panel::ModuleView,64> modules{};
        const int count=mcov::OverlayUiCheck::Modules(overlay,page,modules);
        for(int index=0;index<count;++index) {
            if(modules[index].mode!=mcov::control_panel::TileMode::Toggle) continue;
            SelectSubTab(overlay,page,"",index,count);
            mcov::OverlayUiCheck::Modules(overlay,page,modules); const bool original=modules[index].enabled;
            auto frame=[&] { Draw(overlay,page,0,{1280,800}); };
            auto* root=ImGui::FindWindowByName("mcov control panel");
            const ImVec2 p{root->Pos.x+root->Size.x-159,root->Pos.y+22};
            for(bool expected:{!original,original}) {
                io.AddMousePosEvent(p.x,p.y); frame(); io.AddMouseButtonEvent(0,true); frame(); io.AddMouseButtonEvent(0,false); frame(); frame();
                mcov::OverlayUiCheck::Modules(overlay,page,modules);
                Require(modules[index].enabled==expected,"Compact module toggle did not update real module state");
            }
        }
    }
    SelectSubTab(overlay,Page::Visuals,"",0,26);
    auto* root=ImGui::FindWindowByName("mcov control panel");
    io.AddMousePosEvent(-100,-100); Draw(overlay,Page::Visuals,0,{1280,800});
    ImGui::FocusWindow(root); ImGui::SetNavID(root->GetID("Enabled##module_enabled"),ImGuiNavLayer_Main,0,ImRect({0,0},{50,50}));
    ImGui::GetCurrentContext()->NavCursorVisible=true;
    const bool box=mcov::OverlayUiCheck::Box(overlay);
    io.AddKeyEvent(ImGuiKey_Space,true); Draw(overlay,Page::Visuals,0,{1280,800});
    io.AddKeyEvent(ImGuiKey_Space,false); Draw(overlay,Page::Visuals,0,{1280,800});
    Require(mcov::OverlayUiCheck::Box(overlay)!=box,"Keyboard could not activate the compact toggle");
    mcov::OverlayUiCheck::SetModule(overlay,mcov::keys::Action::BoxEsp,box);
}
void SettingsWindowChecks(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& output) {
    auto& io=ImGui::GetIO();
    SelectSubTab(overlay,Page::Settings,"",2,4);
    auto* content=Child("/content_"); ImGui::SetScrollY(content,150);
    Draw(overlay,Page::Settings,0,{1280,800}); Draw(overlay,Page::Settings,0,{1280,800});
    Require(content->Scroll.y>0,"Keybind scrolling is unavailable");
    SelectSubTab(overlay,Page::Visuals,"",0,26);
    Require(Child("/content_")->Scroll.y==0,"Changing module did not reset content scroll");
    auto* root=ImGui::FindWindowByName("mcov control panel"); const auto original=root->Pos;
    const ImVec2 grip{original.x+root->Size.x*.5f,original.y+20};
    auto frame=[&] { Draw(overlay,Page::Visuals,0,{1280,800}); };
    io.AddMousePosEvent(grip.x,grip.y); frame(); io.AddMouseButtonEvent(0,true); frame();
    io.AddMousePosEvent(grip.x+35,grip.y+15); frame(); io.AddMouseButtonEvent(0,false); frame();
    Require(root->Pos.x==original.x+35&&root->Pos.y==original.y+15,"Compact menu drag lost its offset");
    mcov::OverlayUiCheck::Center(overlay); io.AddMousePosEvent(-100,-100);
    SelectSubTab(overlay,Page::Combat,"",1,8,{480,360});
    SaveFrame(device,context,output/"module-grid-small.png");
    SelectSubTab(overlay,Page::Combat,"",1,8,{1920,1080}); mcov::OverlayUiCheck::Center(overlay);
    Draw(overlay,Page::Combat,0,{1920,1080}); Draw(overlay,Page::Combat,0,{1920,1080});
    SaveFrame(device,context,output/"module-settings-large.png");
    mcov::OverlayUiCheck::Center(overlay);
}
void SearchChecks(mcov::Overlay& overlay) {
    SelectSubTab(overlay,Page::Combat,"",0,8);
    auto frame=[&] { Draw(overlay,Page::Combat,0,{1280,800}); };
    auto& io=ImGui::GetIO(); auto* root=ImGui::FindWindowByName("mcov control panel");
    const ImVec2 p{root->Pos.x+45,root->Pos.y+58};
    io.AddMousePosEvent(p.x,p.y); frame(); io.AddMouseButtonEvent(0,true); frame(); io.AddMouseButtonEvent(0,false); frame();
    io.AddInputCharactersUTF8("AURA"); frame(); frame(); frame();
    auto* tabs=ModuleTabs(Page::Combat);
    Require(tabs&&tabs->Tabs.Size==1&&mcov::OverlayUiCheck::Menu(overlay).settingsSection==0,"Case-insensitive module search failed");
    io.AddInputCharactersUTF8("zzzz"); frame(); frame();
    auto* content=Child("/content_");
    Require(content->DrawList->VtxBuffer.Size>0,"Empty search lost its recovery UI");
    io.AddKeyEvent(ImGuiKey_Escape,true); frame(); io.AddKeyEvent(ImGuiKey_Escape,false); frame();
    // Search resets when the user chooses another category.
    Draw(overlay,Page::Visuals,0,{1280,800});
    auto* nav=Child("/navigation_"); const ImVec2 item{nav->Pos.x+50,nav->Pos.y+29*.5f+35};
    io.AddMousePosEvent(item.x,item.y); Draw(overlay,Page::Visuals,0,{1280,800},true);
    io.AddMouseButtonEvent(0,true); Draw(overlay,Page::Visuals,0,{1280,800},true);
    io.AddMouseButtonEvent(0,false); Draw(overlay,Page::Visuals,0,{1280,800},true);
    Require(!mcov::OverlayUiCheck::Menu(overlay).search[0],"Category navigation retained a stale search");
    io.AddMousePosEvent(-100,-100); Draw(overlay,Page::Visuals,0,{1280,800});
}
void GlassScenePreview(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& output) {
    // Kontrollierter Hintergrund: keine Desktop-Aufnahme, keine Spiel-/Benutzerdaten.
    for(auto size:{ImVec2(1280,800),ImVec2(480,360)}) {
        const int w=int(size.x),h=int(size.y); std::vector<ImU32> pixels(w*h);
        for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
            const float u=float(x)/w,v=float(y)/h;
            if(v<.45f) pixels[y*w+x]=IM_COL32(100+int(u*35),153+int(u*25),195,255);
            else {
                const int patch=((x/90)+(y/65))%2;
                pixels[y*w+x]=IM_COL32(55+patch*45,100+patch*45,58+patch*25,255);
            }
            if((u>.13f&&u<.26f&&v>.13f&&v<.75f)||(u>.65f&&u<.79f&&v>.27f&&v<.68f))
                pixels[y*w+x]=IM_COL32(91,68,45,255);
            if((u>.82f&&u<.94f&&v>.19f&&v<.39f)) pixels[y*w+x]=IM_COL32(230,219,199,255);
        }
        D3D11_TEXTURE2D_DESC d{}; d.Width=w; d.Height=h; d.MipLevels=d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.data(),UINT(w*4),0};
        Hr(device->CreateTexture2D(&d,&data,&previewScene));
        Hr(device->CreateShaderResourceView(previewScene.Get(),nullptr,&previewSceneImage));
        mcov::OverlayUiCheck::CloseDetails(overlay); mcov::OverlayUiCheck::Center(overlay);
        Draw(overlay,Page::Combat,0,size); ImGui::SetScrollY(Child("/content_"),0); Draw(overlay,Page::Combat,0,size); Draw(overlay,Page::Combat,0,size);
        const char* suffix=w<500?"small":"desktop";
        SaveFrame(device,context,output/(std::string("glass-scene-grid-")+suffix+".png"));
        SelectSubTab(overlay,Page::Combat,"combat_sections",1,8,size);
        Draw(overlay,Page::Combat,0,size); SaveFrame(device,context,output/(std::string("glass-scene-settings-")+suffix+".png"));
        previewSceneImage.Reset(); previewScene.Reset();
    }
    mcov::OverlayUiCheck::CloseDetails(overlay); mcov::OverlayUiCheck::Center(overlay);
}
void MarkerMenuChecks(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,
                      const std::filesystem::path& output) {
    auto& io=ImGui::GetIO(); auto& ctx=*ImGui::GetCurrentContext();
    auto& config=mcov::OverlayUiCheck::CubeSettings(overlay);
    auto frame=[&] { Draw(overlay,Page::Visuals,0,{1280,800}); };
    mcov::OverlayUiCheck::Center(overlay); frame(); frame();
    SelectSubTab(overlay,Page::Visuals,"visual_sections",8,26);
    auto* content=Child("/content_");
    auto click=[&](ImVec2 p) {
        io.AddMousePosEvent(p.x,p.y); frame(); io.AddMouseButtonEvent(0,true); frame();
        io.AddMouseButtonEvent(0,false); frame(); frame();
    };
    auto controls=[&]() -> ImGuiTable* {
        auto* table=ctx.Tables.GetByKey(Child("/content_")->GetID("marker_controls"));
        Require(table!=nullptr,"Marker controls are missing"); return table;
    };
    const auto top=controls()->OuterRect.Min;
    click({top.x+32,top.y+8});
    Require(!config.enabled,"Enable marker checkbox did not update renderer config");
    click({top.x+32,top.y+8});
    Require(config.enabled,"Enable marker checkbox did not re-enable renderer config");
    // Die erste Größenzeile nutzt denselben echten Slider wie Target Aura.
    const float frameHeight=mcov::style::MenuFont()->FontSize+6;
    const float row=frameHeight+2*controls()->RowCellPaddingY;
    const auto& column=controls()->Columns[1];
    const float sliderY=controls()->OuterRect.Min.y+row+controls()->RowCellPaddingY+frameHeight*.5f;
    click({column.WorkMaxX-10,sliderY});
    Require(config.sizeFraction>1&&config.sizeFraction<=1.5f,"Marker size slider was not wired to renderer config");
    // CTRL-Klick muss einen sichtbaren Editor oeffnen, statt alle Glyphen zu verstecken.
    io.AddKeyEvent(ImGuiMod_Ctrl,true);
    click({column.WorkMinX+60,sliderY});
    Require(ctx.TempInputId!=0,"CTRL-click did not open numeric slider input");
    auto* input=ImGui::GetInputTextState(ctx.TempInputId);
    Require(input&&input->TextLen>0,"Numeric slider input lost its value");
    bool visibleInput=false;
    const ImVec2 editPoint{column.WorkMinX+60,sliderY};
    const auto white=ImGui::GetIO().Fonts->TexUvWhitePixel;
    for(const auto& v:content->DrawList->VtxBuffer)
        if(v.pos.x>=column.WorkMinX&&v.pos.x<=column.WorkMaxX&&v.pos.y>editPoint.y-9&&v.pos.y<editPoint.y+9&&
           (v.uv.x!=white.x||v.uv.y!=white.y)&&
           (v.col&IM_COL32_A_MASK)!=0&&((v.col>>IM_COL32_R_SHIFT)&255)>220) visibleInput=true;
    Require(visibleInput,"Numeric slider input text was transparent");
    io.AddKeyEvent(ImGuiMod_Ctrl,false); io.AddKeyEvent(ImGuiKey_Escape,true); frame();
    io.AddKeyEvent(ImGuiKey_Escape,false); frame();
    config.glow=.4f; config.animate=false; config.showDepth=false; config.lime={.85f,.15f,1,1};
    mcov::OverlayUiCheck::ResetVisuals(overlay);
    Require(config.enabled&&config.animate&&config.showDepth&&config.glow==1&&config.sizeFraction==.68f&&config.lime.y==1,
            "Visuals reset did not restore marker defaults");
    frame(); frame();
    SaveFrame(device,context,output/"marker-settings.png");
    mcov::OverlayUiCheck::MarkerDetails(overlay,true); frame(); frame();
    content=Child("/content_"); ImGui::SetScrollY(content,content->ScrollMax.y); frame(); frame();
    Require(controls()->OuterRect.Max.y<=content->InnerRect.Max.y,"Expanded marker controls were unreachable by scrolling");
    SaveFrame(device,context,output/"marker-settings-details.png");
    mcov::OverlayUiCheck::Center(overlay);
    for(int i=0;i<3;++i) Draw(overlay,Page::Visuals,0,{480,360});
    content=Child("/content_");
    ImGui::SetScrollY(content,0);
    for(int i=0;i<3;++i) Draw(overlay,Page::Visuals,0,{480,360});
    Require(mcov::OverlayUiCheck::Menu(overlay).settingsSection==8,"Marker selection did not survive compact layout");
    SaveFrame(device,context,output/"marker-settings-small.png");
    mcov::OverlayUiCheck::MarkerDetails(overlay,false);
    ImGui::SetScrollY(Child("/content_"),0);
    io.AddMousePosEvent(-100,-100); mcov::OverlayUiCheck::Center(overlay); frame(); frame();
}
void WorldVisualChecks(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& output) {
    auto& config=mcov::OverlayUiCheck::WorldSettings(overlay);
    mcov::WorldSnapshot scene{}; scene.health=scene.maxHealth=20; scene.camY=1.8;
    scene.pitchDeg=8; scene.fovDeg=65; scene.nearPlane=.05f; scene.farPlane=1024;
    scene.entityCount=1; scene.hasTarget=1; scene.targetEntityId=42;
    auto& e=scene.entities[0]; e.entityId=42; e.uuid={11,22}; e.z=5;
    e.width=.6f; e.height=e.modelHeight=1.8f; e.health=e.maxHealth=20;
    e.flags=mcov::kEntityIsLiving|mcov::kEntityIsPlayer; e.rigKind=mcov::kRigHumanoid;
    mcov::CopyField(e.name,"Effect preview");
    config.trails.enabled=config.hits.enabled=config.ambient.enabled=true;
    config.bubbles.enabled=config.kills.enabled=config.crumbs.enabled=true;
    config.trails.subjects=mcov::TrailSubjects::Target;
    config.ambient.amount=80; config.ambient.radius=5;
    for(int i=0;i<90;++i) {
        scene.frameIndex++; e.x=-2+i*.035; e.z=5+std::sin(i*.04)*.6;
        scene.posX=-1.5f; scene.posZ=3+i*.015f;
        e.distance=float(std::sqrt(e.x*e.x+e.z*e.z));
        if(i==83) e.health=18;
        mcov::OverlayUiCheck::UpdateWorld(overlay,scene,1+i*.04);
    }
    const auto render=[&](ImVec2 size) {
        auto& io=ImGui::GetIO(); io.DisplaySize=size; io.DeltaTime=1.f/60;
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        mcov::OverlayUiCheck::InitGlow(overlay,device);
        mcov::WorldVisualsRenderer::Prepare(*ImGui::GetBackgroundDrawList());
        mcov::OverlayUiCheck::DrawEsp(overlay,scene);
        const auto& draw=*ImGui::GetBackgroundDrawList();
        for(const auto& vertex:draw.VtxBuffer) Require(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y),"World preview generated invalid vertices");
        Require(draw.VtxBuffer.Size>100,"World visual preview is empty"); ImGui::Render();
    };
    render({1280,720}); SaveFrame(device,context,output/"world-visuals.png");
    config.trails.shape=mcov::TrailShape::Dots; config.hits.shape=mcov::ParticleShape::Stars;
    config.ambient.shape=mcov::AmbientShape::Stars;
    render({1280,720}); SaveFrame(device,context,output/"world-visuals-stars.png");
    render({480,360}); SaveFrame(device,context,output/"world-visuals-small.png");
    ++scene.frameIndex; e.health=0;
    mcov::OverlayUiCheck::UpdateWorld(overlay,scene,4.6);
    ++scene.frameIndex; mcov::OverlayUiCheck::UpdateWorld(overlay,scene,4.78);
    render({1280,720}); SaveFrame(device,context,output/"world-kill-effect.png");
    // Echter Self-Record, perspektivisch getrennte F5-Kamera; ESP bleibt nur am Fremdspieler.
    scene.entityCount=2; auto& self=scene.entities[1]; self=e;
    self.entityId=99; self.uuid={33,44}; self.x=-1.5; self.z=4;
    self.health=20; self.headPitch=12; self.flags|=mcov::kEntityIsSelf;
    render({1280,720}); SaveFrame(device,context,output/"world-own-hat.png");
    render({480,360}); SaveFrame(device,context,output/"world-own-hat-small.png");
    auto& hat=mcov::OverlayUiCheck::HatSettings(overlay);
    bool* enabled[]={&config.trails.enabled,&config.hits.enabled,&config.ambient.enabled,&config.bubbles.enabled,
        &config.kills.enabled,&config.crumbs.enabled,&hat.enabled};
    for(int section=9;section<=15;++section) {
        SelectSubTab(overlay,Page::Visuals,"visual_sections",section,26);
        const char* names[]={"trail_settings","hit_particle_settings","ambient_particle_settings","hit_bubble_settings",
            "kill_effect_settings","breadcrumb_settings","local_hat_settings"};
        auto* table=ImGui::GetCurrentContext()->Tables.GetByKey(Child("/content_")->GetID(names[section-9]));
        Require(table!=nullptr,"New visual settings were not reachable");
        auto& io=ImGui::GetIO(); const auto top=table->OuterRect.Min;
        auto frame=[&] { Draw(overlay,Page::Visuals,0,{1280,800}); };
        io.AddMousePosEvent(top.x+30,top.y+8); frame(); io.AddMouseButtonEvent(0,true); frame();
        io.AddMouseButtonEvent(0,false); frame();
        Require(!*enabled[section-9],"Visual checkbox did not update real renderer configuration");
        io.AddMouseButtonEvent(0,true); frame(); io.AddMouseButtonEvent(0,false); frame();
        Require(*enabled[section-9],"Visual checkbox did not re-enable the effect");
        io.AddMousePosEvent(-100,-100); frame();
        SaveFrame(device,context,output/(std::string("world-settings-")+std::to_string(section-9)+".png"));
    }
    const auto& binds=mcov::OverlayUiCheck::Binds(overlay);
    for(int index=mcov::keys::Index(mcov::keys::Action::Trails);index<mcov::keys::kCount;++index)
        Require(!binds[index].key,"New visual binds collided with existing Minecraft keys");
    // Neue Keybind-Aktionen schalten die tatsächlichen Module, inklusive des eigenen Huts.
    mcov::keys::Input input{}; mcov::OverlayUiCheck::Input(overlay,input);
    const mcov::keys::Action actions[]={mcov::keys::Action::HitBubbles,mcov::keys::Action::KillEffect,
        mcov::keys::Action::BreadCrumbs,mcov::keys::Action::ChinaHat};
    for(int i=0;i<4;++i) {
        auto& binding=mcov::OverlayUiCheck::Binds(overlay)[mcov::keys::Index(actions[i])];
        binding={0x78+i,0,mcov::keys::Mode::Toggle};
        input[0x78+i]=true; mcov::OverlayUiCheck::Input(overlay,input);
        Require(!*enabled[3+i],"New keybind failed to disable real visual configuration");
        input={}; mcov::OverlayUiCheck::Input(overlay,input); input[0x78+i]=true;
        mcov::OverlayUiCheck::Input(overlay,input); Require(*enabled[3+i],"New keybind failed to re-enable real configuration");
        input={}; mcov::OverlayUiCheck::Input(overlay,input); binding={};
    }
    mcov::OverlayUiCheck::ResetVisuals(overlay);
    Require(!config.Enabled()&&hat.enabled&&!hat.firstPerson,"Visual reset retained effects or removed the default F5 hat");
}
void RockstarChecks(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& output) {
    using mcov::rockstar::Feature;
    auto& config=mcov::OverlayUiCheck::Rockstar(overlay); auto& pack=mcov::OverlayUiCheck::Pack(overlay);
    mcov::WorldSnapshot scene{}; scene.camY=1.6; scene.fovDeg=70; scene.nearPlane=.05f; scene.farPlane=1024;
    scene.health=scene.maxHealth=20; scene.hasTarget=1; scene.targetEntityId=42; scene.entityCount=3;
    mcov::CopyField(scene.dimension,"Overworld"); mcov::CopyField(scene.serverAddress,"Local world");
    auto& entity=scene.entities[0]; entity.entityId=42; entity.uuid={1,2}; entity.z=4;
    entity.health=entity.maxHealth=20; entity.distance=4; entity.width=.6f; entity.height=entity.modelHeight=1.8f;
    entity.flags=mcov::kEntityIsLiving|mcov::kEntityIsPlayer; entity.rigKind=mcov::kRigHumanoid; mcov::CopyField(entity.name,"Glow target");
    scene.entities[1]=entity; scene.entities[1].entityId=43; scene.entities[1].uuid={3,4}; scene.entities[1].x=10; scene.entities[1].z=-4; scene.entities[1].distance=11;
    scene.entities[2]=entity; scene.entities[2].entityId=44; scene.entities[2].uuid={5,6}; scene.entities[2].x=-1.5; scene.entities[2].z=5; scene.entities[2].flags|=mcov::kEntityIsSelf;
    // Freund-Zuordnung wird durch echte Mittelklick-Flanken geaendert, nie durch gehaltene Buttons oder das Menue.
    config[Feature::MiddleFriend].enabled=true; mcov::OverlayUiCheck::Snapshot(overlay,scene); Draw(overlay,Page::Misc,0,{1280,800});
    mcov::keys::Input input{}; mcov::OverlayUiCheck::Input(overlay,input); input[VK_MBUTTON]=true; mcov::OverlayUiCheck::Input(overlay,input);
    Require(mcov::combat::Lookup(mcov::OverlayUiCheck::CombatEditor(overlay).Relations().Read(),entity.uuid)==mcov::combat::Relation::Friend,"Middle click did not add crosshair player as friend");
    mcov::OverlayUiCheck::Input(overlay,input);
    Require(mcov::combat::Lookup(mcov::OverlayUiCheck::CombatEditor(overlay).Relations().Read(),entity.uuid)==mcov::combat::Relation::Friend,"Held middle button toggled a friend twice");
    mcov::OverlayUiCheck::Input(overlay,{}); mcov::OverlayUiCheck::Input(overlay,input,true);
    Require(mcov::combat::Lookup(mcov::OverlayUiCheck::CombatEditor(overlay).Relations().Read(),entity.uuid)==mcov::combat::Relation::Friend,"Menu click changed friend relation");
    mcov::OverlayUiCheck::Input(overlay,{}); mcov::OverlayUiCheck::Input(overlay,input);
    Require(mcov::combat::Lookup(mcov::OverlayUiCheck::CombatEditor(overlay).Relations().Read(),entity.uuid)==mcov::combat::Relation::Neutral,"Second middle click did not remove friend");
    mcov::OverlayUiCheck::Input(overlay,{});
    for(int i=0;i<=int(Feature::Arrows);++i) config.modules[i].enabled=true;
    config[Feature::TargetCircle].mode=1; config[Feature::DeathCoordinates].enabled=config[Feature::Waypoints].enabled=true;
    pack.ResetWorld(); const double now=double(GetTickCount64())/1000;
    scene.frameIndex=1; pack.Update(scene,now-.1,config,42);
    scene.frameIndex=2; scene.posY=.2f; scene.health=16; entity.health=12; pack.Update(scene,now-.05,config,42);
    scene.posX=2; scene.posZ=5; Require(pack.AddMarker(scene,"Waypoint preview"),"preview waypoint missing");
    auto render=[&](ImVec2 size) {
        auto& io=ImGui::GetIO(); io.DisplaySize=size; io.DeltaTime=1.f/60; ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        mcov::OverlayUiCheck::InitGlow(overlay,device);
        auto* draw=ImGui::GetBackgroundDrawList(); const auto clips=draw->_ClipRectStack.Size;
        mcov::OverlayUiCheck::DrawEsp(overlay,scene);
        Require(draw->_ClipRectStack.Size==clips&&draw->VtxBuffer.Size>0,"new visual pack lost clipping or emitted no geometry");
        for(const auto& v:draw->VtxBuffer) Require(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y),"new visual generated nonfinite vertices");
        ImGui::Render();
    };
    render({1280,800}); SaveFrame(device,context,output/"rockstar-effects.png");
    render({480,360}); SaveFrame(device,context,output/"rockstar-effects-small.png");
    // Eigene Bodenansicht: Der Ring muss auch ohne ESP/Wings ruhig und weich wirken.
    config={}; config[Feature::JumpCircles].enabled=true; config[Feature::JumpCircles].auraColours=false;
    config[Feature::JumpCircles].start={.38f,1,.62f,1}; config[Feature::JumpCircles].end={.68f,1,.8f,1};
    scene.entityCount=0; scene.camX=0; scene.camY=2.1; scene.camZ=-2.8; scene.yawDeg=0; scene.pitchDeg=37;
    scene.posX=scene.posZ=scene.posY=0; scene.health=20; pack.ResetWorld();
    scene.frameIndex=10; pack.Update(scene,now,config,0); scene.posY=.2f; scene.frameIndex++; pack.Update(scene,now+.05,config,0);
    const auto jumpPreview=[&](double age,const char* name) {
        auto& io=ImGui::GetIO(); io.DisplaySize={900,600}; io.DeltaTime=1.f/60; ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        const mcov::CameraProjection camera(scene,900,600); auto& draw=*ImGui::GetBackgroundDrawList();
        for(int z=-5;z<6;++z) for(int x=-5;x<6;++x) {
            const std::array<mcov::WorldPoint,4> corners={mcov::WorldPoint{double(x),0,double(z)},{double(x+1),0,double(z)},{double(x+1),0,double(z+1)},{double(x),0,double(z+1)}};
            std::array<ImVec2,4> points{}; bool visible=true;
            for(int i=0;i<4;++i) { mcov::ScreenPoint p; if(!camera.Point(corners[i],p)) {visible=false;break;} points[i]={p.x,p.y}; }
            if(visible) draw.AddConvexPolyFilled(points.data(),4,(x+z)%2?IM_COL32(39,44,39,255):IM_COL32(46,51,46,255));
        }
        pack.Update(scene,now+.05+age,config,0);
        mcov::OverlayUiCheck::DrawEsp(overlay,scene); ImGui::Render(); SaveFrame(device,context,output/name);
    };
    jumpPreview(.14,"jump-soft-enter.png"); jumpPreview(.45,"jump-soft-ring.png"); jumpPreview(.95,"jump-soft-fade.png");
    for(int i=0;i<=int(Feature::Arrows);++i) config.modules[i].enabled=true;
    for(int index:{18,20,21,24,25}) {
        SelectSubTab(overlay,Page::Visuals,"visual_sections",index,26);
        SaveFrame(device,context,output/(std::string("rockstar-settings-")+std::to_string(index)+".png"));
    }
    config={}; pack.ResetWorld(); pack.ClearMarkers(); mcov::OverlayUiCheck::Seed(overlay);
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
    // Der Produktionspfad muss Freunde auch ohne ESP und Target-Aura markieren.
    entity.flags=mcov::kEntityIsPlayer|mcov::kEntityIsLiving;
    entity.health=20; entity.uuid={11,22}; scene.hasTarget=0;
    mcov::CopyField(entity.name,"Friend");
    auto& friends=mcov::OverlayUiCheck::CombatEditor(overlay).Relations();
    Require(friends.Set(entity.uuid,mcov::combat::Relation::Friend),"preview friend could not be added");
    auto& io=ImGui::GetIO(); io.DisplaySize={1280,800}; io.DeltaTime=1.f/60;
    ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
    mcov::OverlayUiCheck::InitGlow(overlay,device); ImGui::Render();
    Require(frame({1280,800})>0,"friend hat production path was empty");
    SaveFrame(device,context,output/"friend-hat.png");
    entity.pose=mcov::kPoseCrouching; entity.height=1.5f;
    Require(frame({585,745})>0,"crouching friend lost its hat");
    SaveFrame(device,context,output/"friend-hat-crouched.png");
    entity.pose=mcov::kPoseStanding; entity.height=1.8f;
    config.boxEsp=config.skeleton=config.tracers=config.nameTags=config.healthBars=config.distanceText=config.targetInfo=false;
    Require(frame({585,745})>0,"friend hat depended on ESP or target aura enablement");
    friends.Set(entity.uuid,mcov::combat::Relation::Neutral);
    Require(frame({585,745})==0,"removed friend kept its production hat");
    config=mcov::VisualSettings{};
}
void TargetCubePreview(mcov::Overlay& overlay,ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& output) {
    auto& v=mcov::OverlayUiCheck::Visuals(overlay); const auto saved=v; v={};
    v.boxEsp=v.skeleton=v.tracers=v.nameTags=v.healthBars=v.distanceText=v.targetInfo=false;
    mcov::WorldSnapshot scene{}; scene.camY=.95; scene.fovDeg=65; scene.nearPlane=.05f; scene.farPlane=1024; scene.entityCount=1;
    auto& e=scene.entities[0]; e.entityId=42; e.uuid={11,22}; e.z=3.3; e.width=.6f; e.height=1.8f; e.health=20;
    e.flags=mcov::kEntityIsPlayer|mcov::kEntityIsLiving; e.distance=3.3f; mcov::CopyField(e.name,"Kill Aura target");
    auto frame=[&](ImVec2 viewport) {
        auto& io=ImGui::GetIO(); io.DisplaySize=viewport; io.DeltaTime=1.f/60;
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        mcov::TargetCubeRenderer::Prepare(*ImGui::GetBackgroundDrawList());
        mcov::OverlayUiCheck::DrawEsp(overlay,scene); const int count=ImGui::GetBackgroundDrawList()->VtxBuffer.Size;
        ImGui::Render(); return count;
    };
    mcov::OverlayUiCheck::CubeReport(overlay,e);
    Require(frame({585,745})>0,"live target cube required ESP/Aura to be enabled");
    const auto startAngle=mcov::OverlayUiCheck::CubeAngle(overlay);
    for(int i=0;i<20;++i) frame({585,745});
    Require(std::abs(mcov::OverlayUiCheck::CubeAngle(overlay)-startAngle)>.1,"production cube rotation was blocked by Windows menu animations");
    SaveFrame(device,context,output/"target-cube.png");
    auto& marker=mcov::OverlayUiCheck::CubeSettings(overlay);
    marker.enabled=false;
    Require(frame({585,745})==0,"disabled production marker still rendered");
    marker.enabled=true; marker.opacity=0;
    Require(frame({585,745})==0,"production marker ignored opacity");
    marker.opacity=1; marker.animate=false; frame({585,745});
    const auto frozen=mcov::OverlayUiCheck::CubeAngle(overlay);
    for(int i=0;i<10;++i) frame({585,745});
    Require(mcov::OverlayUiCheck::CubeAngle(overlay)==frozen,"production marker ignored animation checkbox");
    marker.lime={.65f,.12f,1,1}; marker.yellow={.1f,.9f,1,1}; marker.glow=1.4f;
    marker.sizeFraction=.85f; marker.showDepth=false; marker.strokeFraction=.03f;
    frame({585,745}); SaveFrame(device,context,output/"target-cube-custom.png");
    marker=mcov::TargetCubeConfig{};
    v.boxEsp=v.skeleton=v.nameTags=v.healthBars=true;
    frame({1280,800}); SaveFrame(device,context,output/"target-cube-esp.png");
    v.boxEsp=v.skeleton=v.nameTags=v.healthBars=false;
    mcov::OverlayUiCheck::ReduceMotion(overlay,true); frame({480,360});
    const auto angle=mcov::OverlayUiCheck::CubeAngle(overlay);
    for(int i=0;i<15;++i) frame({480,360});
    Require(mcov::OverlayUiCheck::CubeAngle(overlay)==angle,"production cube ignored reduced motion");
    SaveFrame(device,context,output/"target-cube-small.png");
    mcov::OverlayUiCheck::ReduceMotion(overlay,false);
    mcov::OverlayUiCheck::CubeReport(overlay,e,mcov::CombatBridgeStatus::Active,mcov::kCombatLeaseMs+1);
    Require(frame({585,745})==0,"stale combat report retained target cube");
    mcov::OverlayUiCheck::CubeReport(overlay,e,mcov::CombatBridgeStatus::Paused);
    Require(frame({585,745})==0,"paused Kill Aura retained target cube");
    mcov::OverlayUiCheck::CubeReport(overlay,e); e.uuid={33,44};
    Require(frame({585,745})==0,"reused entity ID received another UUID's target cube");
    e.uuid={11,22}; e.health=0;
    Require(frame({585,745})==0,"dead entity retained target cube");
    mcov::OverlayUiCheck::StopCube(overlay); v=saved;
}
}
void LauncherChecks(ID3D11Device* device,ID3D11DeviceContext* context,const std::filesystem::path& output) {
    mcov::launcher::View view;
    auto frame=[&](ImVec2 size) {
        auto& io=ImGui::GetIO(); io.DisplaySize=size; io.DeltaTime=1.f/60;
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        if(previewGlass) previewGlass->BeginFrame(nullptr,size,true,false);
        auto& ctx=*ImGui::GetCurrentContext(); const auto before=ImGui::GetStyle();
        const int colors=ctx.ColorStack.Size,vars=ctx.StyleVarStack.Size,windows=ctx.CurrentWindowStack.Size,fonts=ctx.FontStack.Size;
        auto actions=mcov::launcher::Render(view,size);
        Require(ctx.ColorStack.Size==colors&&ctx.StyleVarStack.Size==vars&&ctx.CurrentWindowStack.Size==windows&&ctx.FontStack.Size==fonts,"Launcher leaked ImGui state");
        for(int i=0;i<ImGuiCol_COUNT;++i)
            Require(std::memcmp(&before.Colors[i],&ImGui::GetStyle().Colors[i],sizeof(ImVec4))==0,"Launcher changed global palette");
        ImGui::Render(); Require(ImGui::GetDrawData()->TotalVtxCount>0,"Empty launcher");
        return actions;
    };
    auto screenshot=[&](const char* name,ImVec2 size) {
        frame(size); frame(size); SaveFrame(device,context,output/name);
    };
    screenshot("launcher-empty.png",{640,520});
    view.instances={{1234,"Minecraft* 1.21.11","Java 21",false,false},
                    {5678,"Minecraft 26.2 - Sehr langer Fenstertitel mit Profil und Server","Java 25",true,true}};
    view.selected=0;
    screenshot("launcher-ready.png",{640,520});
    screenshot("launcher-small.png",{480,380});
    view.busy=true; view.phase="Agent verbinden und Live-Daten pruefen...";
    screenshot("launcher-busy.png",{640,520});
    view.busy=false; view.error=view.needsJdk=true;
    view.status="Ein passendes JDK 21 fehlt. Waehle deinen JDK-Ordner aus und starte erneut.";
    screenshot("launcher-error.png",{640,520});
    view.tab=1; view.diagnostic="Verbindung zu PID 1234 konnte nicht vorbereitet werden.";
    view.log="[INFO] Minecraft gefunden\n[WARN] Ein passendes JDK 21 fehlt.";
    screenshot("launcher-diagnose.png",{480,380});
    // Echte Eingabe prueft den einzigen Start-Button; der Test startet keinen Prozess.
    view.tab=0; view.error=view.needsJdk=false;
    frame({640,520}); auto* content=Child("/launcher_content");
    ImGui::SetScrollY(content,0); frame({640,520}); frame({640,520});
    const ImVec2 point{320,content->Pos.y+content->Size.y+25};
    auto& io=ImGui::GetIO(); io.AddMousePosEvent(point.x,point.y); frame({640,520});
    io.AddMouseButtonEvent(0,true); frame({640,520}); io.AddMouseButtonEvent(0,false);
    Require(frame({640,520}).start,"Launcher start button did not respond");
    view.busy=true; io.AddMouseButtonEvent(0,true); frame({640,520}); io.AddMouseButtonEvent(0,false);
    Require(!frame({640,520}).start,"Launcher allowed duplicate starts while connecting");
    io.AddMousePosEvent(-100,-100);
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
        mcov::LiquidGlass glass;
        Require(glass.Initialize(device.Get(),context.Get()),"Liquid glass shaders failed to initialize");
        previewGlass=&glass;
        // Die erweiterte Fixture plus mehrere große Snapshots passen nicht auf den 1-MB-Teststack.
        auto overlayStorage=std::make_unique<mcov::Overlay>(); auto& overlay=*overlayStorage; mcov::OverlayUiCheck::Seed(overlay);
        const Page pages[]={Page::Combat,Page::Visuals,Page::Blocks,Page::Hud,Page::Stats,Page::Settings,Page::Misc};
        auto frame=[&](Page page,bool preserve=false) { Draw(overlay,page,0,{1280,800},preserve); };
        auto& editor=mcov::OverlayUiCheck::CombatEditor(overlay);
        editor.Select(mcov::CombatMenu::Section::AimAssist); mcov::OverlayUiCheck::AllowCombat(overlay);
        constexpr const char* sectionIds[]={"combat_sections","visual_sections","block_sections","hud_sections","stats_sections","settings_sections","misc_sections"};
        constexpr int sectionCounts[]={8,26,7,20,3,4,8};
        GlassScenePreview(overlay,device.Get(),context.Get(),output);
        ModuleTileChecks(overlay);
        SettingsWindowChecks(overlay,device.Get(),context.Get(),output);
        SearchChecks(overlay);
        for(int p=0;p<IM_ARRAYSIZE(pages);++p) {
            for(int section=0;section<sectionCounts[p];++section)
                SelectSubTab(overlay,pages[p],sectionIds[p],section,sectionCounts[p]);
            SelectSubTab(overlay,pages[p],sectionIds[p],0,sectionCounts[p]);
        }
        mcov::OverlayUiCheck::CloseDetails(overlay);
        frame(Page::Combat); ImGui::SetScrollY(Child("/content_"),0); frame(Page::Combat);
        editor.Select(mcov::CombatMenu::Section::AimAssist); frame(Page::Combat); frame(Page::Combat);
        frame(Page::Combat); SaveFrame(device.Get(),context.Get(),output/"classic-combat.png");
        SaveFrame(device.Get(),context.Get(),output/"module-grid-combat.png");
        frame(Page::Visuals); SaveFrame(device.Get(),context.Get(),output/"classic-visuals.png");
        frame(Page::Hud); SaveFrame(device.Get(),context.Get(),output/"classic-hud-settings.png");
        auto click=[&](ImVec2 p,Page page=Page::Visuals) {
            io.AddMousePosEvent(p.x,p.y); frame(page,true); io.AddMouseButtonEvent(0,true); frame(page,true);
            io.AddMouseButtonEvent(0,false); frame(page,true); frame(page,true);
        };
        SelectSubTab(overlay,Page::Visuals,"visual_sections",0,26);
        SaveFrame(device.Get(),context.Get(),output/"module-settings-box.png");
        frame(Page::Visuals); auto* content=Child("/content_"); ImGui::SetScrollY(content,0); frame(Page::Visuals);
        auto* boxTable=ImGui::GetCurrentContext()->Tables.GetByKey(content->GetID("box_controls"));
        Require(boxTable!=nullptr,"Box ESP settings are missing");
        const ImVec2 checkbox{boxTable->OuterRect.Min.x+23,boxTable->OuterRect.Min.y+8};
        click(checkbox); Require(!mcov::OverlayUiCheck::Box(overlay),"Classic checkbox did not update real ESP setting");
        mcov::OverlayUiCheck::ResetVisuals(overlay); Require(mcov::OverlayUiCheck::Box(overlay),"Visuals reset failed");
        mcov::OverlayUiCheck::CloseDetails(overlay); frame(Page::Visuals); frame(Page::Visuals);
        auto* nav=Child("/navigation_");
        const float tabHeight=29;
        click({nav->Pos.x+nav->Size.x*.5f,nav->Pos.y+tabHeight*.5f});
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Combat,"Left navigation did not select Combat");
        nav=Child("/navigation_"); click({nav->Pos.x+nav->Size.x*.5f,nav->Pos.y+5*(tabHeight+6)+tabHeight*.5f},Page::Combat);
        Require(mcov::OverlayUiCheck::Menu(overlay).page==Page::Settings,"Left navigation did not select Settings");
        auto* panel=ImGui::FindWindowByName("mcov control panel"); const auto original=panel->Pos;
        const ImVec2 grip{original.x+panel->Size.x*.5f,original.y+12};
        io.AddMousePosEvent(grip.x,grip.y); frame(Page::Settings); io.AddMouseButtonEvent(0,true); frame(Page::Settings);
        Require(mcov::OverlayUiCheck::Menu(overlay).dragging,"Classic title bar did not start drag");
        io.AddMousePosEvent(grip.x+55,grip.y+35); frame(Page::Settings); io.AddMouseButtonEvent(0,false); frame(Page::Settings);
        Require(panel->Pos.x==original.x+55&&panel->Pos.y==original.y+35,"Classic menu drag lost its offset");
        mcov::OverlayUiCheck::Center(overlay); frame(Page::Settings);
        SelectSubTab(overlay,Page::Settings,"settings_sections",2,4);
        auto& ctx=*ImGui::GetCurrentContext(); content=Child("/content_");
        SaveFrame(device.Get(),context.Get(),output/"classic-keybinds.png");
        auto* keysTable=ctx.Tables.GetByKey(content->GetID("keybind_editor"));
        Require(keysTable!=nullptr,"Keybind editor not rendered");
        const float bindRow=keysTable->RowPosY2-keysTable->RowPosY1;
        const float keyHeader=mcov::style::MenuFont()->FontSize+2*keysTable->RowCellPaddingY;
        click({keysTable->Columns[1].WorkMinX+50,keysTable->OuterRect.Min.y+keyHeader+3*bindRow+bindRow*.5f},Page::Settings);
        Require(mcov::OverlayUiCheck::Capture(overlay)==mcov::keys::Index(mcov::keys::Action::KillAura),"Real bind button did not enter capture");
        mcov::keys::Input input{}; input[0x4B]=input[0x11]=true;
        mcov::OverlayUiCheck::Input(overlay,input,true);
        auto& binds=mcov::OverlayUiCheck::Binds(overlay); const int auraKey=mcov::keys::Index(mcov::keys::Action::KillAura);
        Require(binds[auraKey].key==0x4B&&binds[auraKey].modifiers==mcov::keys::Ctrl,"Key capture failed to store Ctrl+K");
        mcov::OverlayUiCheck::Input(overlay,{}); mcov::OverlayUiCheck::Input(overlay,input);
        Require(editor.Store().Read().values.enabled,"Captured key did not toggle the production Kill Aura store");
        std::array<mcov::control_panel::ModuleView,64> liveTiles{};
        mcov::OverlayUiCheck::Modules(overlay,Page::Combat,liveTiles);
        Require(liveTiles[0].enabled&&std::string(liveTiles[0].binding).find("K")!=std::string::npos,"Tiles did not reflect the keybind toggle and binding");
        mcov::OverlayUiCheck::Input(overlay,{});
        mcov::OverlayUiCheck::Capture(overlay,mcov::keys::Action::Skeleton); mcov::OverlayUiCheck::Input(overlay,input,true);
        Require(mcov::OverlayUiCheck::Capture(overlay)>=0&&!binds[mcov::keys::Index(mcov::keys::Action::Skeleton)].key,"Conflicting keybind was accepted");
        mcov::OverlayUiCheck::Input(overlay,{},true);
        Require(!binds[mcov::keys::Index(mcov::keys::Action::Skeleton)].key,"Rejected chord silently rebound its modifier on release");
        input={}; input[0x1B]=true; mcov::OverlayUiCheck::Input(overlay,input,true);
        Require(mcov::OverlayUiCheck::Capture(overlay)<0,"Escape failed to cancel binding capture");
        mcov::OverlayUiCheck::Capture(overlay,mcov::keys::Action::Skeleton);
        input={}; input[0x10]=input[0xA0]=true; mcov::OverlayUiCheck::Input(overlay,input,true);
        Require(mcov::OverlayUiCheck::Capture(overlay)>=0,"Standalone modifier was captured before release");
        mcov::OverlayUiCheck::Input(overlay,{},true);
        Require(binds[mcov::keys::Index(mcov::keys::Action::Skeleton)].key==0xA0,"Standalone modifier capture did not finish on release");
        mcov::OverlayUiCheck::Input(overlay,{}); mcov::OverlayUiCheck::ResetCombat(overlay);
        const auto prefs=output/"client-preferences-test.ini"; mcov::OverlayUiCheck::PrefPath(overlay,prefs);
        mcov::OverlayUiCheck::Settings(overlay).occludeWorldEffects=false;
        auto& savedCrystal=mcov::OverlayUiCheck::Crystal(overlay); auto& savedAnchor=mcov::OverlayUiCheck::Anchor(overlay);
        savedCrystal.GrimConservative(); savedCrystal.requireTotem=true; savedCrystal.placeMs=350;
        savedAnchor.chargeWithOffhandFuel=true; savedAnchor.detonationSlot=8; savedAnchor.minHealth=9;
        binds[auraKey].mode=mcov::keys::Mode::Hold;
        auto& hud=mcov::OverlayUiCheck::Hud(overlay); hud.effects=false; hud.layout[2]={{.4f,.7f},1.3f,true};
        auto& extra=mcov::OverlayUiCheck::Rockstar(overlay); auto& pack=mcov::OverlayUiCheck::Pack(overlay);
        extra[mcov::rockstar::Feature::Wings].enabled=true; extra[mcov::rockstar::Feature::Wings].size=1.7f;
        extra[mcov::rockstar::Feature::Wings].auraColours=false; extra[mcov::rockstar::Feature::Wings].end={.1f,.2f,.3f,.4f};
        hud.extras[0]=true; binds[mcov::keys::Index(mcov::keys::Action::Wings)]={0x47,0,mcov::keys::Mode::Hold};
        mcov::WorldSnapshot markerScene{}; markerScene.posX=-123; markerScene.posY=64; markerScene.posZ=29;
        mcov::CopyField(markerScene.dimension,"Overworld"); mcov::CopyField(markerScene.serverAddress,"Local world");
        Require(pack.AddMarker(markerScene,"Home \"one\""),"test waypoint could not be saved");
        mcov::OverlayUiCheck::Save(overlay); binds=mcov::keys::Defaults(); hud={}; extra={}; pack.ClearMarkers();
        savedCrystal={}; savedAnchor={};
        mcov::OverlayUiCheck::Settings(overlay).occludeWorldEffects=true;
        mcov::OverlayUiCheck::Load(overlay);
        Require(!mcov::OverlayUiCheck::Settings(overlay).occludeWorldEffects,"World depth preference did not survive save/load");
        Require(savedCrystal.strictConfirmation&&savedCrystal.requireCrosshairHit&&savedCrystal.requireTotem&&savedCrystal.placeMs==350&&savedCrystal.maxRange==3&&
                savedAnchor.chargeWithOffhandFuel&&savedAnchor.detonationSlot==8&&savedAnchor.minHealth==9&&!savedAnchor.enabled&&!savedCrystal.enabled,
                "combat settings must persist without auto-enabling modules");
        Require(binds[auraKey].key==0x4B&&binds[auraKey].mode==mcov::keys::Mode::Hold&&!hud.effects&&
                hud.layout[2].placed&&hud.layout[2].position.x==.4f&&hud.layout[2].scale==1.3f,"HUD/bind preferences did not survive save/load");
        Require(extra[mcov::rockstar::Feature::Wings].enabled&&extra[mcov::rockstar::Feature::Wings].size==1.7f&&extra[mcov::rockstar::Feature::Wings].end.w==.4f&&hud.extras[0]&&
                binds[mcov::keys::Index(mcov::keys::Action::Wings)].mode==mcov::keys::Mode::Hold&&pack.Markers()[0].p.x==-123&&std::string(pack.Markers()[0].label.data())=="Home \"one\"","new effects / HUD / quoted waypoint preferences did not survive save/load");
        // Panic muss auch vorher aktive Hold-Module abschalten; Loslassen darf sie nicht wiederherstellen.
        mcov::OverlayUiCheck::Input(overlay,{}); input={}; input[0x47]=true; mcov::OverlayUiCheck::Input(overlay,input);
        mcov::OverlayUiCheck::Panic(overlay); input={}; mcov::OverlayUiCheck::Input(overlay,input);
        for(int i=3;i<mcov::keys::kCount;++i) if(!mcov::keys::System(static_cast<mcov::keys::Action>(i)))
            Require(!mcov::OverlayUiCheck::Module(overlay,static_cast<mcov::keys::Action>(i)),"Panic left a module enabled");
        Require(pack.Markers()[0].used,"Panic erased a saved waypoint");
        mcov::OverlayUiCheck::ResetVisuals(overlay); mcov::OverlayUiCheck::SetModule(overlay,mcov::keys::Action::Hud,true);
        extra={}; pack.ClearMarkers();
        mcov::OverlayUiCheck::PrefPath(overlay,{}); std::filesystem::remove(prefs); binds=mcov::keys::Defaults(); hud={};
        savedCrystal={}; savedAnchor={};
        for(auto size:{ImVec2(720,560),ImVec2(480,360)}) for(Page page:pages) { Draw(overlay,page,0,size); Draw(overlay,page,0,size); }
        SaveFrame(device.Get(),context.Get(),output/"classic-small.png");
        MarkerMenuChecks(overlay,device.Get(),context.Get(),output);
        WorldVisualChecks(overlay,device.Get(),context.Get(),output);
        RockstarChecks(overlay,device.Get(),context.Get(),output);
        mcov::WorldSnapshot hudScene{}; hudScene.health=hudScene.maxHealth=20; hudScene.fps=144; hudScene.hasTarget=1;
        hudScene.targetDistance=2.4f; hudScene.targetHealth=16; hudScene.targetMaxHealth=20;
        mcov::CopyField(hudScene.targetName,"Target player"); mcov::CopyField(hudScene.gameVersion,"1.21.11");
        hudScene.posX=124; hudScene.posY=64; hudScene.posZ=-285; hudScene.effectCount=2;
        mcov::CopyField(hudScene.effects[0].name,"Speed"); hudScene.effects[0].durationTicks=2400;
        mcov::CopyField(hudScene.effects[1].name,"Strength"); hudScene.effects[1].durationTicks=900;
        for(int i=0;i<mcov::kSlotCount;++i) { hudScene.equipment[i].present=1; mcov::CopyField(hudScene.equipment[i].name,"Diamond item"); hudScene.equipment[i].maxDamage=300; hudScene.equipment[i].damage=i*30; }
        binds[auraKey]={0x4B,0,mcov::keys::Mode::Toggle}; hud.performance=true;
        auto hudFrame=[&](ImVec2 size,bool edit=true) {
            io.DisplaySize=size; io.DeltaTime=1.f/60; ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            const auto before=ImGui::GetStyle(); const auto vars=ctx.StyleVarStack.Size,colors=ctx.ColorStack.Size,fonts=ctx.FontStack.Size;
            mcov::OverlayUiCheck::DrawHud(overlay,hudScene,edit);
            Require(vars==ctx.StyleVarStack.Size&&colors==ctx.ColorStack.Size&&fonts==ctx.FontStack.Size&&
                    std::memcmp(&before,&ImGui::GetStyle(),sizeof(ImGuiStyle))==0,"HUD editor leaked global style or fonts"); ImGui::Render();
        };
        hudFrame({1280,800}); hudFrame({1280,800});
        SaveFrame(device.Get(),context.Get(),output/"modular-hud-editor.png");
        const auto rect=mcov::OverlayUiCheck::HudRects(overlay)[2]; const auto before=hud.layout[2].position;
        const ImVec2 pick{rect.min.x+25,rect.min.y+30};
        io.AddMousePosEvent(pick.x,pick.y); hudFrame({1280,800}); io.AddMouseButtonEvent(0,true); hudFrame({1280,800});
        io.AddMousePosEvent(pick.x+100,pick.y+20); hudFrame({1280,800}); io.AddMouseButtonEvent(0,false); hudFrame({1280,800});
        Require(hud.layout[2].position.x>before.x,"HUD editor drag did not move the coordinates module");
        io.AddMousePosEvent(-100,-100); hudFrame({480,360}); hudFrame({480,360});
        for(const auto& r:mcov::OverlayUiCheck::HudRects(overlay)) Require(r.min.x>=0&&r.min.y>=0&&r.max.x<=480.01f&&r.max.y<=360.01f,"HUD module escaped small viewport");
        SaveFrame(device.Get(),context.Get(),output/"modular-hud-small.png");
        hud.layout[2]={}; hudFrame({1280,800},false); SaveFrame(device.Get(),context.Get(),output/"modular-hud.png");
        hud.extras.fill(true); extra[mcov::rockstar::Feature::Notifications].enabled=extra[mcov::rockstar::Feature::DetectPlayer].enabled=true;
        mcov::keys::Input hudKeys{}; hudKeys[0x57]=hudKeys[1]=true; pack.Input(hudKeys,true,double(GetTickCount64())/1000);
        pack.Notify("Wings enabled",double(GetTickCount64())/1000); pack.Notify("Waypoint saved",double(GetTickCount64())/1000);
        hudScene.entityCount=1; auto& nearby=hudScene.entities[0]; nearby.entityId=50; nearby.uuid={1,50}; nearby.distance=12;
        nearby.health=18; nearby.x=8; nearby.z=-280; nearby.flags=mcov::kEntityIsPlayer|mcov::kEntityIsLiving; mcov::CopyField(nearby.name,"Nearby player");
        hudFrame({1280,800},false); SaveFrame(device.Get(),context.Get(),output/"rockstar-hud.png");
        hudFrame({480,360}); hudFrame({480,360});
        for(const auto& r:mcov::OverlayUiCheck::HudRects(overlay)) Require(r.min.x>=0&&r.min.y>=0&&r.max.x<=480.01f&&r.max.y<=360.01f,"new HUD escaped a small viewport");
        hudFrame({1280,800},false);
        extra={}; pack.ResetWorld();
        hud={}; binds=mcov::keys::Defaults(); io.AddMousePosEvent(-100,-100);
        EspChecks(overlay,device.Get(),context.Get(),output);
        TargetCubePreview(overlay,device.Get(),context.Get(),output);
        auto auraFrame=[&] {
            io.DisplaySize={1280,800}; io.DeltaTime=1.f/60;
            ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            mcov::OverlayUiCheck::DrawAura(overlay,device.Get()); ImGui::Render();
        };
        // Das Overlay überträgt Bereitschaft/Fokus; Place/Use liest Minecraft selbst.
        mcov::OverlayUiCheck::Crystal(overlay).enabled=true;
        auto& macroBinds=mcov::OverlayUiCheck::Binds(overlay);
        macroBinds[mcov::keys::Index(mcov::keys::Action::CrystalMacro)]={0x43,0,mcov::keys::Mode::Toggle};
        mcov::keys::Input macroInput{}; mcov::OverlayUiCheck::Input(overlay,macroInput);
        Require(mcov::OverlayUiCheck::ItemRequest(overlay,true).crystal.enabled,"macro checkbox did not arm request");
        macroInput[0x43]=true; mcov::OverlayUiCheck::Input(overlay,macroInput);
        Require(mcov::OverlayUiCheck::ItemRequest(overlay,true).inputAllowed&&!mcov::OverlayUiCheck::ItemRequest(overlay,false).inputAllowed,"macro focus lease incorrect");
        macroInput={}; mcov::OverlayUiCheck::Input(overlay,macroInput);
        Require(!mcov::OverlayUiCheck::ItemRequest(overlay,true).crystal.enabled,"macro toggle was replayed on release");
        mcov::OverlayUiCheck::Crystal(overlay).enabled=true;
        mcov::OverlayUiCheck::Binds(overlay)=mcov::keys::Defaults();
        mcov::OverlayUiCheck::ItemStatus(overlay);
        mcov::OverlayUiCheck::Crystal(overlay).GrimConservative();
        mcov::OverlayUiCheck::Anchor(overlay).GrimConservative();
        for(auto size:{ImVec2(1280,800),ImVec2(480,360)}) {
            const auto suffix=size.x<500?"-small.png":".png";
            for(int section:{3,5,6}) {
                SelectSubTab(overlay,Page::Combat,"combat_sections",section,8,size);
                mcov::OverlayUiCheck::Center(overlay);
                for(int i=0;i<20;++i) Draw(overlay,Page::Combat,0,size);
                SaveFrame(device.Get(),context.Get(),output/(std::string("item-settings-")+std::to_string(section)+suffix));
                auto* utilityContent=Child("/content_"); ImGui::SetScrollY(utilityContent,utilityContent->ScrollMax.y);
                Draw(overlay,Page::Combat,0,size); Draw(overlay,Page::Combat,0,size);
            }
            SelectSubTab(overlay,Page::Blocks,"block_sections",3,7,size); mcov::OverlayUiCheck::Center(overlay);
            for(int i=0;i<20;++i) Draw(overlay,Page::Blocks,0,size);
            Require(ImGui::GetCurrentContext()->Tables.GetByKey(Child("/content_")->GetID("chunkfinder_controls"))!=nullptr,"Chunkfinder tab did not render its real controls");
            SaveFrame(device.Get(),context.Get(),output/(std::string("chunkfinder-settings")+suffix));
        }
        auto& finder=mcov::OverlayUiCheck::Chunk(overlay); finder.enabled=true; finder.minHints=2; finder.labels=false;
        mcov::WorldSnapshot chunkScene{}; chunkScene.camY=2; chunkScene.camZ=-4; chunkScene.fovDeg=70; chunkScene.nearPlane=.05f; chunkScene.farPlane=1024;
        chunkScene.blockCount=3;
        chunkScene.blocks[0]={0,0,4,mcov::kBlockRotatedDeepslate,{}};
        chunkScene.blocks[1]={1,0,4,mcov::kBlockRotatedDeepslate,{}};
        chunkScene.blocks[2]={2,0,4,mcov::kBlockDiamond,{}};
        auto chunkFrame=[&] {
            io.DisplaySize={1280,800}; ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
            const int vertices=mcov::OverlayUiCheck::DrawChunk(overlay,chunkScene); ImGui::Render(); return vertices;
        };
        Require(chunkFrame()>0,"qualified deepslate chunk did not render");
        SaveFrame(device.Get(),context.Get(),output/"chunkfinder-blocks.png");
        finder.minHints=3; Require(chunkFrame()==0,"chunk threshold counted ordinary ores");
        finder={};
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
        LauncherChecks(device.Get(),context.Get(),output);
        previewGlass=nullptr; glass.Shutdown();
        ImGui_ImplDX11_Shutdown(); ImGui::DestroyContext(); CoUninitialize();
        std::cout<<"PASS control panel: translucent client, 7 categories / 76 module tabs, real toggles, search, style ownership, drag, marker settings, key capture/conflicts, saved HUD layout, ESP/friend hats/aura/Rockstar pack and glass launcher\n";
        return 0;
    } catch (const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}

