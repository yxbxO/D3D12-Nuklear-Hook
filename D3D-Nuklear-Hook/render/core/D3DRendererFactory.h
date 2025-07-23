#pragma once

class D3DRendererFactory {
public:
    static std::unique_ptr<ID3DRenderer> create_renderer(D3DVersion version, IDXGISwapChain* swap_chain) {
        switch (version) {
            case D3DVersion::D3D12:
                return std::make_unique<D3D12Renderer>(swap_chain);
            case D3DVersion::D3D11:
                return std::make_unique<D3D11Renderer>(swap_chain);
            default:
                return nullptr;
        }
    }

    static D3DVersion detect_version() {
        // Check for D3D12
        auto d3d12 = mem::module(L"d3d12.dll");
        if (d3d12.begin() && d3d12.get_export("D3D12CreateDevice")) {
            return D3DVersion::D3D12;
        }

        // Check for D3D11
        auto d3d11 = mem::module(L"d3d11.dll");
        if (d3d11.begin() && d3d11.get_export("D3D11CreateDevice")) {
            return D3DVersion::D3D11;
        }
        // D3D11 support will be added later

        mem::d_log("[D3DVersion detect_version] UNKNOWN D3D VERION");
        return D3DVersion::Unknown;
    }
}; 