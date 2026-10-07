#include "depth_renderer.hpp"
#include "imgui_impl_dx11.h"
#include <d3dcompiler.h>
#include <algorithm>

namespace mcov {
namespace {
constexpr char kDepthShader[]=R"(
cbuffer vertexBuffer:register(b0) { float4x4 ProjectionMatrix; };
cbuffer DepthParams:register(b1) { float2 viewSize; float zNear,zFar; uint vertexOffset,valid; float bias,relativeBias; };
struct Input { float2 pos:POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; float inverseDepth:TEXCOORD1; };
struct Pixel { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; float inverseDepth:TEXCOORD1; };
Pixel vs(Input v) { Pixel p; p.pos=mul(ProjectionMatrix,float4(v.pos,0,1)); p.col=v.col; p.uv=v.uv; p.inverseDepth=v.inverseDepth; return p; }
Texture2D image:register(t0); Texture2D<float> sceneDepth:register(t1); SamplerState imageSampler:register(s0);
float4 ps(Pixel p):SV_Target {
    float4 color=p.col*image.Sample(imageSampler,p.uv);
    if(p.inverseDepth>0) {
        if(!valid) discard;
        uint w,h; sceneDepth.GetDimensions(w,h);
        int2 xy=clamp(int2(p.pos.xy/viewSize*float2(w,h)),int2(0,0),int2(w-1,h-1));
        // Konservative Nachbarprobe schliesst kleine Loecher an Spieler-/Blockkanten.
        float z=sceneDepth.Load(int3(xy,0));
        z=min(z,sceneDepth.Load(int3(clamp(xy+int2(1,0),int2(0,0),int2(w-1,h-1)),0)));
        z=min(z,sceneDepth.Load(int3(clamp(xy+int2(0,1),int2(0,0),int2(w-1,h-1)),0)));
        float surface=zNear*zFar/max(zNear,zFar-z*(zFar-zNear));
        float effect=1/p.inverseDepth;
        clip(surface+bias+relativeBias*effect-effect);
    }
    return color;
})";
}
bool DepthRenderer::Initialize(ID3D11Device* device) {
    Shutdown(); if(!device) return false;
    Com<ID3DBlob> vs,ps,error;
    if(FAILED(D3DCompile(kDepthShader,sizeof(kDepthShader)-1,nullptr,nullptr,nullptr,"vs","vs_4_0",0,0,&vs,&error)) ||
       FAILED(D3DCompile(kDepthShader,sizeof(kDepthShader)-1,nullptr,nullptr,nullptr,"ps","ps_4_0",0,0,&ps,&error))) return false;
    if(FAILED(device->CreateVertexShader(vs->GetBufferPointer(),vs->GetBufferSize(),nullptr,&vertex_)) ||
       FAILED(device->CreatePixelShader(ps->GetBufferPointer(),ps->GetBufferSize(),nullptr,&pixel_))) return false;
    const D3D11_INPUT_ELEMENT_DESC elements[]={
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,UINT(offsetof(ImDrawVert,pos)),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,UINT(offsetof(ImDrawVert,uv)),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,UINT(offsetof(ImDrawVert,col)),D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32_FLOAT,1,0,D3D11_INPUT_PER_VERTEX_DATA,0}};
    if(FAILED(device->CreateInputLayout(elements,4,vs->GetBufferPointer(),vs->GetBufferSize(),&layout_))) return false;
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=kDepthWidth; desc.Height=kDepthHeight; desc.MipLevels=desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R32_FLOAT; desc.SampleDesc.Count=1; desc.Usage=D3D11_USAGE_DYNAMIC;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE; desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&texture_)) || FAILED(device->CreateShaderResourceView(texture_.Get(),nullptr,&depthView_))) return false;
    D3D11_BUFFER_DESC buffer{}; buffer.ByteWidth=kVertexCapacity*sizeof(float); buffer.Usage=D3D11_USAGE_DYNAMIC;
    buffer.BindFlags=D3D11_BIND_VERTEX_BUFFER; buffer.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
    if(FAILED(device->CreateBuffer(&buffer,nullptr,&tagBuffer_))) return false;
    buffer.ByteWidth=sizeof(Params); buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(FAILED(device->CreateBuffer(&buffer,nullptr,&constants_))) return false;
    tags_.reserve(kVertexCapacity); return true;
}
void DepthRenderer::Shutdown() { vertex_.Reset(); pixel_.Reset(); layout_.Reset(); texture_.Reset(); depthView_.Reset(); tagBuffer_.Reset(); constants_.Reset(); draw_=nullptr; }
bool DepthRenderer::Finish(ID3D11DeviceContext* context,ImDrawData& data,const DepthFrame* frame) {
    if(!draw_||!vertex_||draw_->VtxBuffer.Size>int(kVertexCapacity)||data.DisplaySize.x<1||data.DisplaySize.y<1) return false;
    bool found=false; vertexOffset_=indexOffset_=0;
    for(const auto* list:data.CmdLists) { if(list==draw_) { found=true; break; } vertexOffset_+=list->VtxBuffer.Size; indexOffset_+=list->IdxBuffer.Size; }
    if(!found||tags_.empty()) return true;
    params_.width=data.DisplaySize.x*data.FramebufferScale.x; params_.height=data.DisplaySize.y*data.FramebufferScale.y;
    params_.nearPlane=frame?frame->nearPlane:.05f; params_.farPlane=frame?frame->farPlane:1000;
    params_.vertexOffset=vertexOffset_; params_.valid=frame!=nullptr;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(frame) {
        if(FAILED(context->Map(texture_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return false;
        for(int y=0;y<kDepthHeight;++y) std::memcpy(static_cast<char*>(mapped.pData)+y*mapped.RowPitch,frame->pixels.data()+y*kDepthWidth,kDepthWidth*sizeof(float));
        context->Unmap(texture_.Get(),0);
    }
    tags_.resize(draw_->VtxBuffer.Size,0.f);
    if(FAILED(context->Map(tagBuffer_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return false;
    std::memcpy(mapped.pData,tags_.data(),tags_.size()*sizeof(float)); context->Unmap(tagBuffer_.Get(),0);
    if(FAILED(context->Map(constants_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped))) return false;
    std::memcpy(mapped.pData,&params_,sizeof params_); context->Unmap(constants_.Get(),0);
    // Gleiche Texturen, Scissors und Blend-Callbacks; nur der Hintergrund bekommt den Depth-Shader.
    for(auto& command:draw_->CmdBuffer) if(!command.UserCallback&&command.ElemCount) { command.UserCallback=Render; command.UserCallbackData=this; }
    return true;
}
void DepthRenderer::Render(const ImDrawList*,const ImDrawCmd* command) {
    auto* self=static_cast<DepthRenderer*>(command->UserCallbackData);
    auto* state=static_cast<ImGui_ImplDX11_RenderState*>(ImGui::GetPlatformIO().Renderer_RenderState);
    if(!self||!state) return; auto* context=state->DeviceContext;
    Com<ID3D11VertexShader> oldVs; Com<ID3D11PixelShader> oldPs;
    Com<ID3D11ShaderResourceView> oldPsView,oldImage;
    Com<ID3D11Buffer> oldVsConstants,oldPsConstants;
    Com<ID3D11InputLayout> oldLayout; context->IAGetInputLayout(&oldLayout);
    Com<ID3D11Buffer> oldVertices,oldTags; UINT strides[2]{},offsets[2]{};
    ID3D11Buffer* priorBuffers[2]{}; context->IAGetVertexBuffers(0,2,priorBuffers,strides,offsets);
    oldVertices.Attach(priorBuffers[0]); oldTags.Attach(priorBuffers[1]);
    context->VSGetShader(&oldVs,nullptr,nullptr); context->PSGetShader(&oldPs,nullptr,nullptr);
    context->PSGetShaderResources(1,1,&oldPsView); context->PSGetShaderResources(0,1,&oldImage);
    context->VSGetConstantBuffers(1,1,&oldVsConstants); context->PSGetConstantBuffers(1,1,&oldPsConstants);
    context->VSSetShader(self->vertex_.Get(),nullptr,0); context->PSSetShader(self->pixel_.Get(),nullptr,0);
    context->IASetInputLayout(self->layout_.Get());
    // Zweiter Vertex-Stream statt SV_VertexID: lokale und globale ImGui-Offsets sind eindeutig.
    ID3D11Buffer* buffers[]={oldVertices.Get(),self->tagBuffer_.Get()};
    const UINT ownStrides[]={strides[0],sizeof(float)},ownOffsets[]={offsets[0]+UINT(self->vertexOffset_)*strides[0],0};
    context->IASetVertexBuffers(0,2,buffers,ownStrides,ownOffsets);
    auto* depth=self->depthView_.Get(); auto* constants=self->constants_.Get();
    context->PSSetShaderResources(1,1,&depth);
    context->VSSetConstantBuffers(1,1,&constants); context->PSSetConstantBuffers(1,1,&constants);
    auto* image=reinterpret_cast<ID3D11ShaderResourceView*>(command->GetTexID()); context->PSSetShaderResources(0,1,&image);
    const auto* data=ImGui::GetDrawData(); const auto off=data->DisplayPos,scale=data->FramebufferScale;
    const D3D11_RECT rect{LONG((command->ClipRect.x-off.x)*scale.x),LONG((command->ClipRect.y-off.y)*scale.y),
                          LONG((command->ClipRect.z-off.x)*scale.x),LONG((command->ClipRect.w-off.y)*scale.y)};
    if(rect.right>rect.left&&rect.bottom>rect.top) {
        context->RSSetScissorRects(1,&rect);
        context->DrawIndexed(command->ElemCount,command->IdxOffset+self->indexOffset_,command->VtxOffset);
    }
    context->VSSetShader(oldVs.Get(),nullptr,0); context->PSSetShader(oldPs.Get(),nullptr,0);
    context->IASetInputLayout(oldLayout.Get());
    context->IASetVertexBuffers(0,2,priorBuffers,strides,offsets);
    auto* psView=oldPsView.Get(); auto* priorImage=oldImage.Get();
    context->PSSetShaderResources(1,1,&psView); context->PSSetShaderResources(0,1,&priorImage);
    auto* vsConstants=oldVsConstants.Get(); auto* psConstants=oldPsConstants.Get();
    context->VSSetConstantBuffers(1,1,&vsConstants); context->PSSetConstantBuffers(1,1,&psConstants);
}
}
