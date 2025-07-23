#include "../../pch.h"
#include <queue>

#define USER_TEXTURES 6
#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_INDEX_BUFFER 128 * 1024

#define NK_INCLUDE_COMMAND_USERDATA
#define NK_INCLUDE_STANDARD_BOOL
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_ASSERT

#include "../Nuklear/nuklear.h"

#define NK_D3D12_IMPLEMENTATION
#include "../Nuklear/nuklear_d3d12.h"


// Private implementation details
static HANDLE g_hSwapChainWaitableObject = nullptr;
static WNDPROC OriginalWndProc = nullptr;
static bool menu_is_open = true;

// Simple command execution for presentation layer only
static ID3D12Device* g_device = nullptr;
static ID3D12CommandAllocator* g_command_allocator = nullptr;
static ID3D12GraphicsCommandList* g_command_list = nullptr;
static ID3D12Fence* g_fence = nullptr;
static UINT64 g_fence_value = 0;
static HANDLE g_fence_event = nullptr;

LRESULT D3D12Renderer::wndproc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (GetAsyncKeyState(VK_INSERT) & 1) {
        menu_is_open = !menu_is_open;
    }

    if (!m_nk_ctx)
        return CallWindowProcW(m_originalWndProc, hWnd, msg, wParam, lParam);

    m_inputQueue.push({ hWnd, msg, wParam, lParam });
   
    bool shouldCapture = menu_is_open && nk_item_is_any_active(m_nk_ctx);

    if (shouldCapture && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN ||
                          msg == WM_RBUTTONUP || msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
                          msg == WM_MOUSEWHEEL || msg == WM_MOUSEMOVE || msg == WM_KEYDOWN ||
                          msg == WM_KEYUP || msg == WM_CHAR)) {
        return 0;
    }

    return CallWindowProcW(m_originalWndProc, hWnd, msg, wParam, lParam);
}

static void execute_commands() {
    auto* renderer = static_cast<D3D12Renderer*>(g_renderer.get());

    g_command_list->Close();
    renderer->command_queue()->ExecuteCommandLists(
        1, reinterpret_cast<ID3D12CommandList* const*>(&g_command_list));

    const auto current_fence_value = ++g_fence_value;
    renderer->command_queue()->Signal(g_fence, current_fence_value);
    if (g_fence->GetCompletedValue() < current_fence_value) {
        g_fence->SetEventOnCompletion(current_fence_value, g_fence_event);
        WaitForSingleObject(g_fence_event, INFINITE);
    }

    g_command_allocator->Reset();
    g_command_list->Reset(g_command_allocator, nullptr);
}

void D3D12Renderer::release_swap_chain_buffers() {
    if (!m_rtv_buffer_count || !m_rtv_buffers)
        return;

    mem::d_log("[D3D12Renderer] Releasing swap_chain buffers");

    // Release all render target buffers
    for (UINT i = 0; i < m_rtv_buffer_count; i++) {
        if (m_rtv_buffers[i]) {
            mem::d_log("[D3D12Renderer] Releasing buffer: {}", i);
            m_rtv_buffers[i]->Release();
            m_rtv_buffers[i] = nullptr;
        }
    }

    mem::d_log("[D3D12Renderer] Release swap_chain buffers complete");
}

void D3D12Renderer::get_swap_chain_buffers(UINT width, UINT height) {
    if (!m_rtv_buffer_count || !m_rtv_descriptor_heap || !g_device)
        return;

    D3D12_CPU_DESCRIPTOR_HANDLE descriptor_handle =
        m_rtv_descriptor_heap->GetCPUDescriptorHandleForHeapStart();

    mem::d_log("[D3D12Renderer] Allocating {} buffers", m_rtv_buffer_count);

    for (UINT i = 0; i < m_rtv_buffer_count; i++) {
        mem::d_log("[D3D12Renderer] Creating buffer: {}", i);
        HRESULT hr = m_swap_chain->GetBuffer(i, IID_PPV_ARGS(&m_rtv_buffers[i]));
        if (FAILED(hr)) {
            mem::d_log("[D3D12Renderer] GetBuffer {} failed", i);
            continue;
        }

        g_device->CreateRenderTargetView(m_rtv_buffers[i], nullptr, descriptor_handle);
        m_rtv_handles[i] = descriptor_handle;
        descriptor_handle.ptr += m_rtv_desc_increment;
    }

    mem::d_log("[D3D12Renderer] Get swap_chain buffers complete for {} buffers",
               m_rtv_buffer_count);

    if (width || height) {
        nk_d3d12_resize(width, height);
    }
}

bool D3D12Renderer::initialize() {
    if (m_shutdown) {
        mem::d_log("[D3D12Renderer] Initialize: shutdown flag set");
        return false;
    }

    if (m_initialized) {
        return true;
    }

    if (!m_swap_chain) {
        mem::d_log("[D3D12Renderer] Initialize: swap_chain is null");
        return false;
    }

    if (!m_command_queue) {
        mem::d_log("[D3D12Renderer] Initialize: command_queue is null");
        return false;
    }

    // Get device from swap chain
    if (FAILED(m_swap_chain->GetDevice(__uuidof(ID3D12Device), reinterpret_cast<void**>(&g_device)))) {
        mem::d_log("[D3D12Renderer] Initialize: GetDevice failed");
        return false;
    }

    // Get swap chain description for buffer setup
    DXGI_SWAP_CHAIN_DESC sd{};
    if (FAILED(m_swap_chain->GetDesc(&sd))) {
        mem::d_log("[D3D12Renderer] Initialize: GetDesc failed");
        cleanup_device_resources();
        return false;
    }

    // Setup our presentation command resources
    if (!setup_command_resources()) {
        cleanup_device_resources();
        return false;
    }

    // Setup our render target resources (presentation management)
    m_rtv_buffer_count = sd.BufferCount;
    m_rtv_buffers = new ID3D12Resource*[m_rtv_buffer_count]();
    m_rtv_handles = new D3D12_CPU_DESCRIPTOR_HANDLE[m_rtv_buffer_count]();

    // Create descriptor heap for render targets
    if (!setup_render_target_heap()) {
        cleanup_presentation_resources();
        cleanup_device_resources();
        return false;
    }

    mem::d_log("[D3D12Renderer] BufferCount: {}", m_rtv_buffer_count);

    // Setup swap chain waitable object
    g_hSwapChainWaitableObject = m_swap_chain->GetFrameLatencyWaitableObject();

    // Get swap chain buffers
    get_swap_chain_buffers();

    // Get detailed swap chain description for window setup
    DXGI_SWAP_CHAIN_DESC1 sd1{};
    if (FAILED(m_swap_chain->GetDesc1(&sd1))) {
        mem::d_log("[D3D12Renderer] Initialize: GetDesc1 failed");
        return false;
    }

    // Setup window procedure hook
    setup_window_hook(sd.OutputWindow);

    mem::d_log("[D3D12Renderer] Window: {}x{} handle: {:#x}", sd1.Width, sd1.Height,
               reinterpret_cast<uintptr_t>(sd.OutputWindow));
    mem::d_log("[D3D12Renderer] OriginalWndProc: {:#x}",
               reinterpret_cast<uintptr_t>(OriginalWndProc));

    // Initialize nuklear
    m_nk_ctx = nk_d3d12_init(g_device, sd1.Width, sd1.Height, MAX_VERTEX_BUFFER, MAX_INDEX_BUFFER,
                             USER_TEXTURES);
    if (!m_nk_ctx) {
        mem::d_log("[D3D12Renderer] Initialize: nk_d3d12_init failed");
        return false;
    }

    // Setup fonts
    setup_nuklear_fonts();


    mem::d_log("[D3D12Renderer] Initialize success");
    m_initialized = true;
    return true;
}

void D3D12Renderer::start_input() {
    // Begin Nuklear input frame
    nk_input_begin(m_nk_ctx);

    // Process all queued input events
    // if (auto locked = m_inputMutex.try_lock(); locked)
    {
        while (!m_inputQueue.empty()) {
            const auto& evt = m_inputQueue.front();
            nk_d3d12_handle_event(evt.hwnd, evt.msg, evt.wparam, evt.lparam);
            m_inputQueue.pop();
        }
        // m_inputMutex.unlock();            
    }

    // End Nuklear input frame
    nk_input_end(m_nk_ctx);
}

void D3D12Renderer::remove_window_hook() {
    if (m_window && m_originalWndProc) {
        SetWindowLongPtr(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalWndProc));
        m_window = nullptr;
        m_originalWndProc = nullptr;
    }
}

void D3D12Renderer::shutdown() {
    if (!m_initialized && !m_shutdown)
        return;

    m_mutex.lock();

    m_shutdown = true;

    // Unhook window proc if hooked
    remove_window_hook();

    // Clean up nuklear
    if (m_nk_ctx) {
        nk_d3d12_shutdown();
        m_nk_ctx = nullptr;
    }

    // Clean up presentation resources
    cleanup_presentation_resources();

    // Clean up command resources
    cleanup_command_resources();

    // Clean up device
    cleanup_device_resources();

    mem::d_log("[D3D12Renderer] Cleanup success");
    m_initialized = false;
    m_mutex.unlock();
}

void D3D12Renderer::render() {
    if (!m_initialized || !m_nk_ctx || !g_command_list || !g_command_allocator)
        return;

    if (!m_swap_chain || !m_rtv_buffers || !m_rtv_handles) {
        mem::d_log("[D3D12Renderer] Render: missing presentation resources");
        return;
    }

    m_mutex.lock();


    UINT rtv_index = m_swap_chain->GetCurrentBackBufferIndex();

    // Reset our command resources for this frame
    g_command_allocator->Reset();
    g_command_list->Reset(g_command_allocator, nullptr);

    // Transition render target to render state
    D3D12_RESOURCE_BARRIER resource_barrier{};
    resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    resource_barrier.Transition.pResource = m_rtv_buffers[rtv_index];
    resource_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

    g_command_list->ResourceBarrier(1, &resource_barrier);

    // Set our render target
    g_command_list->OMSetRenderTargets(1, &m_rtv_handles[rtv_index], FALSE, nullptr);

    // Let nuklear render to our command list
    nk_d3d12_render(g_command_list, NK_ANTI_ALIASING_ON);

    // Transition render target to present state
    resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_command_list->ResourceBarrier(1, &resource_barrier);

    // Execute commands
    execute_commands();

    m_mutex.unlock();
}

void D3D12Renderer::draw() {
    if (!initialize()) {
        mem::d_log("[D3D12Renderer] Initialize failed, aborting draw");
        return;
    }


    start_input();


    if (!menu_is_open)
        return;

    /* GUI */
    if (nk_begin(m_nk_ctx, "Demo", nk_rect(50, 50, 230, 250),
        NK_WINDOW_BORDER | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE | NK_WINDOW_MINIMIZABLE |
        NK_WINDOW_TITLE)) {
        enum {
            EASY,
            HARD
        };
        static int op = EASY;
        static int property = 20;

        nk_layout_row_static(m_nk_ctx, 30, 80, 1);
        if (nk_button_label(m_nk_ctx, "button")) {
            // Button pressed - implement functionality here
        }
        nk_layout_row_dynamic(m_nk_ctx, 30, 2);
        if (nk_option_label(m_nk_ctx, "easy", op == EASY))
            op = EASY;
        if (nk_option_label(m_nk_ctx, "hard", op == HARD))
            op = HARD;
        nk_layout_row_dynamic(m_nk_ctx, 22, 1);
        nk_property_int(m_nk_ctx, "Compression:", 0, &property, 100, 10, 1);

        nk_layout_row_dynamic(m_nk_ctx, 20, 1);
        nk_label(m_nk_ctx, "background:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(m_nk_ctx, 25, 1);

        static struct nk_colorf bg;
        if (nk_combo_begin_color(m_nk_ctx, nk_rgb_cf(bg),
            nk_vec2(nk_widget_width(m_nk_ctx), 400))) {
            nk_layout_row_dynamic(m_nk_ctx, 120, 1);
            bg = nk_color_picker(m_nk_ctx, bg, NK_RGBA);
            nk_layout_row_dynamic(m_nk_ctx, 25, 1);
            bg.r = nk_propertyf(m_nk_ctx, "#R:", 0, bg.r, 1.0f, 0.01f, 0.005f);
            bg.g = nk_propertyf(m_nk_ctx, "#G:", 0, bg.g, 1.0f, 0.01f, 0.005f);
            bg.b = nk_propertyf(m_nk_ctx, "#B:", 0, bg.b, 1.0f, 0.01f, 0.005f);
            bg.a = nk_propertyf(m_nk_ctx, "#A:", 0, bg.a, 1.0f, 0.01f, 0.005f);
            nk_combo_end(m_nk_ctx);
        }

        nk_end(m_nk_ctx);
    }

    m_mutex.unlock();
}

// Helper function to save screenshot as BMP
static bool save_screenshot_as_bmp(const std::string& filename, void* data, int width, int height, int row_pitch, DXGI_FORMAT format) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // BMP file header (14 bytes) - must be packed
    #pragma pack(push, 1)
    struct BMPFileHeader {
        uint16_t file_type = 0x4D42; // "BM"
        uint32_t file_size = 0;
        uint16_t reserved1 = 0;
        uint16_t reserved2 = 0;
        uint32_t offset_data = 54; // Header size
    };

    // BMP info header (40 bytes) - must be packed
    struct BMPInfoHeader {
        uint32_t size = 40;
        int32_t width = 0;
        int32_t height = 0;
        uint16_t planes = 1;
        uint16_t bit_count = 24; // RGB (24-bit, no alpha)
        uint32_t compression = 0;
        uint32_t size_image = 0;
        int32_t x_pixels_per_meter = 2835; // 72 DPI
        int32_t y_pixels_per_meter = 2835; // 72 DPI
        uint32_t colors_used = 0;
        uint32_t colors_important = 0;
    };
    #pragma pack(pop)

    // Calculate row padding (BMP rows must be 4-byte aligned)
    const int bytes_per_pixel = 3; // RGB
    const int padded_row_size = ((width * bytes_per_pixel + 3) / 4) * 4;
    const int image_size = padded_row_size * height;

    BMPFileHeader file_header;
    BMPInfoHeader info_header;

    info_header.width = width;
    info_header.height = height; // Positive for bottom-up DIB
    info_header.size_image = image_size;
    file_header.file_size = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + image_size;

    // Write headers
    file.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
    file.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));

    // Write pixel data (BMP expects BGR and bottom-up)
    const uint8_t* src_data = static_cast<const uint8_t*>(data);
    
    // Padding bytes for row alignment
    const uint8_t padding[4] = {0, 0, 0, 0};
    const int padding_size = padded_row_size - (width * bytes_per_pixel);
    
    // Write rows in reverse order (bottom-up for BMP)
    for (int y = height - 1; y >= 0; y--) {
        const uint8_t* row_data = src_data + (y * row_pitch);
        
        for (int x = 0; x < width; x++) {
            uint8_t bgr[3];
            
            // Convert based on actual format (BMP always expects BGR order)
            if (format == 28) { // DXGI_FORMAT_R8G8B8A8_UNORM
                const uint8_t* pixel = row_data + (x * 4);
                // RGBA -> BGR
                bgr[0] = pixel[2]; // Blue = source Red
                bgr[1] = pixel[1]; // Green = source Green  
                bgr[2] = pixel[0]; // Red = source Blue
            } else if (format == 87) { // DXGI_FORMAT_B8G8R8A8_UNORM
                const uint8_t* pixel = row_data + (x * 4);
                // BGRA -> BGR (just copy BGR, skip A)
                bgr[0] = pixel[0]; // Blue = source Blue
                bgr[1] = pixel[1]; // Green = source Green  
                bgr[2] = pixel[2]; // Red = source Red
            } else if (format == 24) { // DXGI_FORMAT_R10G10B10A2_UNORM
                // 10-bit per channel format packed in 32 bits
                const uint32_t* pixel32 = reinterpret_cast<const uint32_t*>(row_data + (x * 4));
                uint32_t packed = *pixel32;
                
                // Extract 10-bit components and convert to 8-bit
                uint32_t r = (packed & 0x3FF);        // Bits 0-9: Red
                uint32_t g = (packed >> 10) & 0x3FF;  // Bits 10-19: Green  
                uint32_t b = (packed >> 20) & 0x3FF;  // Bits 20-29: Blue
                // Alpha is bits 30-31, but we ignore it for BMP
                
                // Convert 10-bit (0-1023) to 8-bit (0-255)
                bgr[0] = static_cast<uint8_t>((b * 255) / 1023); // Blue
                bgr[1] = static_cast<uint8_t>((g * 255) / 1023); // Green
                bgr[2] = static_cast<uint8_t>((r * 255) / 1023); // Red
            } else {
                // Default case - assume RGBA format
                const uint8_t* pixel = row_data + (x * 4);
                bgr[0] = pixel[2]; // Blue = source Red
                bgr[1] = pixel[1]; // Green = source Green  
                bgr[2] = pixel[0]; // Red = source Blue
            }
            
            file.write(reinterpret_cast<const char*>(bgr), 3);
        }
        
        // Add padding to align row to 4 bytes
        if (padding_size > 0) {
            file.write(reinterpret_cast<const char*>(padding), padding_size);
        }
    }

    file.close();
    return true;
}

bool D3D12Renderer::take_screenshot(const std::string& filename) {
    if (!m_initialized || !g_device || !m_swap_chain) {
        return false;
    }

    m_mutex.lock();

    // Get current back buffer
    UINT current_buffer_index = 0;
    if (auto swap_chain3 = m_swap_chain) {
        current_buffer_index = swap_chain3->GetCurrentBackBufferIndex();
    }

    ID3D12Resource* back_buffer = m_rtv_buffers[current_buffer_index];
    if (!back_buffer) {
        m_mutex.unlock();
        return false;
    }

    // Get back buffer description
    D3D12_RESOURCE_DESC back_buffer_desc = back_buffer->GetDesc();
    
    // Create readback buffer (CPU accessible)
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_READBACK;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    // Calculate required size for readback buffer
    UINT64 required_size = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
    UINT num_rows;
    UINT64 row_size_in_bytes;
    
    g_device->GetCopyableFootprints(&back_buffer_desc, 0, 1, 0, &footprint, &num_rows, &row_size_in_bytes, &required_size);

    D3D12_RESOURCE_DESC readback_desc = {};
    readback_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readback_desc.Width = required_size;
    readback_desc.Height = 1;
    readback_desc.DepthOrArraySize = 1;
    readback_desc.MipLevels = 1;
    readback_desc.Format = DXGI_FORMAT_UNKNOWN;
    readback_desc.SampleDesc.Count = 1;
    readback_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    readback_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    ID3D12Resource* readback_buffer = nullptr;
    HRESULT hr = g_device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &readback_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&readback_buffer)
    );

    if (FAILED(hr)) {
        readback_buffer->Release();
        m_mutex.unlock();
        return false;
    }

    // Create temporary command allocator and list for screenshot
    ID3D12CommandAllocator* screenshot_allocator = nullptr;
    ID3D12GraphicsCommandList* screenshot_list = nullptr;

    hr = g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&screenshot_allocator));
    if (FAILED(hr)) {
        readback_buffer->Release();
        m_mutex.unlock();
        return false;
    }

    hr = g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, screenshot_allocator, nullptr, IID_PPV_ARGS(&screenshot_list));
    if (FAILED(hr)) {
        screenshot_allocator->Release();
        readback_buffer->Release();
        m_mutex.unlock();
        return false;
    }

    // Transition back buffer to copy source
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = back_buffer;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    screenshot_list->ResourceBarrier(1, &barrier);

    // Copy from back buffer to readback buffer
    D3D12_TEXTURE_COPY_LOCATION src_location = {};
    src_location.pResource = back_buffer;
    src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    src_location.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION dst_location = {};
    dst_location.pResource = readback_buffer;
    dst_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst_location.PlacedFootprint = footprint;

    screenshot_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

    // Transition back buffer back to render target state
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    screenshot_list->ResourceBarrier(1, &barrier);

    // Execute the commands
    screenshot_list->Close();
    m_command_queue->ExecuteCommandLists(1, reinterpret_cast<ID3D12CommandList* const*>(&screenshot_list));

    // Wait for completion
    const auto fence_value = ++g_fence_value;
    m_command_queue->Signal(g_fence, fence_value);
    if (g_fence->GetCompletedValue() < fence_value) {
        g_fence->SetEventOnCompletion(fence_value, g_fence_event);
        WaitForSingleObject(g_fence_event, INFINITE);
    }

    // Map the readback buffer and save to file
    void* mapped_data = nullptr;
    hr = readback_buffer->Map(0, nullptr, &mapped_data);
    if (SUCCEEDED(hr)) {
        // Generate filename if not provided
        std::string final_filename = filename;
        if (final_filename.empty()) {
            // Get current time for filename
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm_buf;
            localtime_s(&tm_buf, &time_t);
            std::stringstream ss;
            ss << "screenshot_" << std::put_time(&tm_buf, "%Y%m%d_%H%M%S") << ".bmp";
            final_filename = ss.str();
        }

        // Save as simple BMP file
        bool save_success = save_screenshot_as_bmp(
            final_filename,
            mapped_data,
            static_cast<int>(footprint.Footprint.Width),
            static_cast<int>(footprint.Footprint.Height),
            static_cast<int>(footprint.Footprint.RowPitch),
            back_buffer_desc.Format
        );

        readback_buffer->Unmap(0, nullptr);

        // Note: save_success indicates if BMP was written successfully
    }

    // Cleanup
    screenshot_list->Release();
    screenshot_allocator->Release();
    readback_buffer->Release();

    m_mutex.unlock();
    return SUCCEEDED(hr);
}

// Private helper methods
bool D3D12Renderer::setup_command_resources() {
    g_fence_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!g_fence_event) {
        mem::d_log("[D3D12Renderer] Setup command resources: fence_event creation failed");
        return false;
    }

    if (FAILED(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) {
        mem::d_log("[D3D12Renderer] Setup command resources: CreateFence failed");
        CloseHandle(g_fence_event);
        g_fence_event = nullptr;
        return false;
    }

    if (FAILED(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                IID_PPV_ARGS(&g_command_allocator)))) {
        mem::d_log("[D3D12Renderer] Setup command resources: CreateCommandAllocator failed");
        g_fence->Release();
        g_fence = nullptr;
        CloseHandle(g_fence_event);
        g_fence_event = nullptr;
        return false;
    }

    if (FAILED(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_command_allocator,
                                           nullptr, IID_PPV_ARGS(&g_command_list)))) {
        mem::d_log("[D3D12Renderer] Setup command resources: CreateCommandList failed");
        g_command_allocator->Release();
        g_command_allocator = nullptr;
        g_fence->Release();
        g_fence = nullptr;
        CloseHandle(g_fence_event);
        g_fence_event = nullptr;
        return false;
    }

    return true;
}

bool D3D12Renderer::setup_render_target_heap() {
    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc_heap_desc{};
    rtv_desc_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc_heap_desc.NumDescriptors = m_rtv_buffer_count;
    rtv_desc_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtv_desc_heap_desc.NodeMask = 1;

    if (FAILED(g_device->CreateDescriptorHeap(&rtv_desc_heap_desc,
                                              IID_PPV_ARGS(&m_rtv_descriptor_heap)))) {
        mem::d_log("[D3D12Renderer] Setup render target heap: CreateDescriptorHeap failed");
        return false;
    }

    m_rtv_desc_increment =
        g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return true;
}

void D3D12Renderer::setup_window_hook(HWND window) {
    if (!window) return;
    if (m_window && m_window != window) {
        // Optionally: unhook previous window if needed
        if (m_originalWndProc) {
            SetWindowLongPtr(m_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_originalWndProc));
        }
    }
    m_window = window;
    m_originalWndProc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc))
    );
}

void D3D12Renderer::setup_nuklear_fonts() {
    struct nk_font_atlas* atlas;
    nk_d3d12_font_stash_begin(&atlas);
    nk_d3d12_font_stash_end(g_command_list);
    execute_commands();
    nk_d3d12_font_stash_cleanup();
}

void D3D12Renderer::cleanup_presentation_resources() {
    if (m_rtv_buffers) {
        for (UINT i = 0; i < m_rtv_buffer_count; i++) {
            if (m_rtv_buffers[i]) {
                m_rtv_buffers[i]->Release();
            }
        }
        delete[] m_rtv_buffers;
        m_rtv_buffers = nullptr;
    }

    if (m_rtv_handles) {
        delete[] m_rtv_handles;
        m_rtv_handles = nullptr;
    }

    if (m_rtv_descriptor_heap) {
        m_rtv_descriptor_heap->Release();
        m_rtv_descriptor_heap = nullptr;
    }

    m_rtv_buffer_count = 0;
}

void D3D12Renderer::cleanup_command_resources() {
    if (g_command_list) {
        g_command_list->Release();
        g_command_list = nullptr;
    }
    if (g_command_allocator) {
        g_command_allocator->Release();
        g_command_allocator = nullptr;
    }
    if (g_fence) {
        g_fence->Release();
        g_fence = nullptr;
    }
    if (g_fence_event) {
        CloseHandle(g_fence_event);
        g_fence_event = nullptr;
    }
}

void D3D12Renderer::cleanup_device_resources() {
    if (g_device) {
        g_device->Release();
        g_device = nullptr;
    }
} 