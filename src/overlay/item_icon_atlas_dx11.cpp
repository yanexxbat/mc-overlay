#include "item_icon_atlas_dx11.hpp"
#include "../common/equipment_icons.hpp"
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>

namespace mcov {
namespace {
class ComScope final {
    HRESULT result_=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
public:
    ~ComScope() { if (SUCCEEDED(result_)) CoUninitialize(); }
    bool Ready() const { return SUCCEEDED(result_) || result_==RPC_E_CHANGED_MODE; }
};
}
void ItemIconAtlasDX11::Shutdown() noexcept {
    if (texture_) { texture_->Release(); texture_=nullptr; }
}
bool ItemIconAtlasDX11::Initialize(ID3D11Device* device) noexcept {
    Shutdown();
    if (!device) return false;
    try {
        const auto module=GetModuleHandleW(nullptr);
        const auto resource=FindResourceW(module,MAKEINTRESOURCEW(102),RT_RCDATA);
        if (!resource) return false;
        const auto size=SizeofResource(module,resource);
        auto* bytes=static_cast<BYTE*>(LockResource(LoadResource(module,resource)));
        if (!bytes || !size) return false;
        ComScope com;
        if (!com.Ready()) return false;
        using Microsoft::WRL::ComPtr;
        ComPtr<IWICImagingFactory> factory;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))) return false;
        ComPtr<IWICStream> stream; ComPtr<IWICBitmapDecoder> decoder;
        ComPtr<IWICBitmapFrameDecode> frame; ComPtr<IWICFormatConverter> converted;
        if (FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(bytes,size)) ||
            FAILED(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder)) ||
            FAILED(decoder->GetFrame(0,&frame)) || FAILED(factory->CreateFormatConverter(&converted)) ||
            FAILED(converted->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return false;
        UINT width{},height{};
        if (FAILED(converted->GetSize(&width,&height)) || width!=equipment_icons::kWidth || height!=equipment_icons::kHeight) return false;
        std::vector<BYTE> pixels(std::size_t(width)*height*4);
        if (FAILED(converted->CopyPixels(nullptr,width*4,UINT(pixels.size()),pixels.data()))) return false;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=width; desc.Height=height; desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA data{pixels.data(),width*4,0};
        ComPtr<ID3D11Texture2D> texture;
        return SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)) &&
               SUCCEEDED(device->CreateShaderResourceView(texture.Get(),nullptr,&texture_));
    } catch (...) { Shutdown(); return false; }
}
} // namespace mcov
