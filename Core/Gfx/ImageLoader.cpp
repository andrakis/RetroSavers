#include "ImageLoader.h"
#include "BmpReader.h"
#include "Host/Log.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

namespace rs::ImageLoader {

namespace {

using Microsoft::WRL::ComPtr;

// EXIF orientation (tag 274) -> WIC transform.
WICBitmapTransformOptions OrientationTransform(USHORT orientation) {
    switch (orientation) {
    case 2: return WICBitmapTransformFlipHorizontal;
    case 3: return WICBitmapTransformRotate180;
    case 4: return WICBitmapTransformFlipVertical;
    case 5: return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
    case 6: return WICBitmapTransformRotate90;
    case 7: return static_cast<WICBitmapTransformOptions>(WICBitmapTransformRotate270 | WICBitmapTransformFlipHorizontal);
    case 8: return WICBitmapTransformRotate270;
    default: return WICBitmapTransformRotate0;
    }
}

std::optional<Image> LoadWic(const std::wstring& path, int maxDim) {
    ComPtr<IWICImagingFactory> factory;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return std::nullopt;
    ComPtr<IWICBitmapDecoder> decoder;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder))) return std::nullopt;
    ComPtr<IWICBitmapFrameDecode> frame;
    if (FAILED(decoder->GetFrame(0, &frame))) return std::nullopt;

    ComPtr<IWICBitmapSource> source = frame;
    UINT w = 0, h = 0;
    if (FAILED(source->GetSize(&w, &h)) || !w || !h) return std::nullopt;

    // Orientation from EXIF (JPEG) or the TIFF IFD; ignore failures (PNG etc. have none).
    USHORT orientation = 1;
    ComPtr<IWICMetadataQueryReader> meta;
    if (SUCCEEDED(frame->GetMetadataQueryReader(&meta))) {
        PROPVARIANT v;
        PropVariantInit(&v);
        static const wchar_t* queries[] = { L"/app1/ifd/{ushort=274}", L"/ifd/{ushort=274}" };
        for (const wchar_t* q : queries) {
            if (SUCCEEDED(meta->GetMetadataByName(q, &v)) && v.vt == VT_UI2) { orientation = v.uiVal; PropVariantClear(&v); break; }
            PropVariantClear(&v);
        }
    }

    if (maxDim > 0 && (static_cast<int>(w) > maxDim || static_cast<int>(h) > maxDim)) {
        float k = static_cast<float>(maxDim) / static_cast<float>(std::max(w, h));
        UINT nw = std::max(1u, static_cast<UINT>(w * k + 0.5f)), nh = std::max(1u, static_cast<UINT>(h * k + 0.5f));
        ComPtr<IWICBitmapScaler> scaler;
        if (SUCCEEDED(factory->CreateBitmapScaler(&scaler)) && SUCCEEDED(scaler->Initialize(source.Get(), nw, nh, WICBitmapInterpolationModeFant))) {
            source = scaler;
            w = nw;
            h = nh;
        }
    }

    WICBitmapTransformOptions xform = OrientationTransform(orientation);
    if (xform != WICBitmapTransformRotate0) {
        // The rotator needs a converted source for some codecs; convert first, then rotate.
        ComPtr<IWICFormatConverter> pre;
        if (SUCCEEDED(factory->CreateFormatConverter(&pre)) &&
            SUCCEEDED(pre->Initialize(source.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            ComPtr<IWICBitmapFlipRotator> rot;
            if (SUCCEEDED(factory->CreateBitmapFlipRotator(&rot)) && SUCCEEDED(rot->Initialize(pre.Get(), xform))) {
                source = rot;
                source->GetSize(&w, &h);
            }
        }
    }

    ComPtr<IWICFormatConverter> conv;
    if (FAILED(factory->CreateFormatConverter(&conv))) return std::nullopt;
    if (FAILED(conv->Initialize(source.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) return std::nullopt;

    Image img(static_cast<int>(w), static_cast<int>(h));
    UINT stride = w * 4;
    if (FAILED(conv->CopyPixels(nullptr, stride, stride * h, reinterpret_cast<BYTE*>(img.pixels.data())))) return std::nullopt;
    return img;
}

} // namespace

std::optional<Image> Load(const std::wstring& path, int maxDim) {
    if (path.empty()) return std::nullopt;
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return std::nullopt;
    if (auto img = LoadWic(path, maxDim)) return img;
    if (auto img = BmpReader::Load(path)) {
        LogLine(L"ImageLoader: WIC failed, BmpReader loaded " + path);
        return img;
    }
    LogLine(L"ImageLoader: could not decode " + path);
    return std::nullopt;
}

} // namespace rs::ImageLoader
