#pragma once
namespace mem {
    /**
     * @brief A class for interacting with loaded modules/DLLs
     * 
     * The module class provides functionality to find and interact with loaded modules,
     * including finding exports, imports, and waiting for modules to load.
     * Supports both wide and narrow string module names.
     */
    class module : public range {
    public:
        /**
         * @brief Initialize a module wrapper with wide string
         * @param name Wide string name of the module to find (e.g. L"kernel32.dll")
         */
        explicit module(std::wstring_view name) noexcept
            : range(address_t{}, 0)
            , m_base_name(name)
            , m_full_path(name)
            , m_loaded(false) {
            try_get_module();
        }

        /**
         * @brief Initialize a module wrapper with narrow string
         * @param name Narrow string name of the module to find (e.g. "kernel32.dll")
         */
        explicit module(std::string_view name) noexcept
            : module(to_wide(name)) {
        }

        /**
         * @brief Initialize a module wrapper for the main executable
         */
        module() noexcept
            : range(address_t{}, 0)
            , m_loaded(false) {
            try_get_module();
        }

        /**
         * @brief Find exported function address by name
         * @param symbol_name Name of the exported function to find
         * @return Function pointer if found, nullptr otherwise
         * @example
         * auto NtCreateFile = module.get_export("NtCreateFile");
         */
        [[nodiscard]] void* get_export(std::string_view symbol_name) const noexcept {
            if (!m_loaded) return nullptr;

            auto* dos = m_base.cast<IMAGE_DOS_HEADER*>();
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

            auto* nt = m_base.at_offset(dos->e_lfanew).cast<IMAGE_NT_HEADERS*>();
            if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

            const auto& export_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            if (export_dir.Size == 0) return nullptr;

            auto* exports = m_base.at_offset(export_dir.VirtualAddress).cast<IMAGE_EXPORT_DIRECTORY*>();
            auto* names = m_base.at_offset(exports->AddressOfNames).cast<DWORD*>();
            auto* ordinals = m_base.at_offset(exports->AddressOfNameOrdinals).cast<WORD*>();
            auto* functions = m_base.at_offset(exports->AddressOfFunctions).cast<DWORD*>();

            for (DWORD i = 0; i < exports->NumberOfNames; i++) {
                const char* func_name = m_base.at_offset(names[i]).cast<const char*>();
                if (symbol_name == func_name) {
                    WORD ordinal = ordinals[i];
                    return m_base.at_offset(functions[ordinal]).cast<void*>();
                }
            }
            return nullptr;
        }

        /**
         * @brief Find imported function address by name
         * @param symbol_name Name of the imported function to find
         * @return Pointer to IAT entry if found, nullptr otherwise
         * @example
         * auto CreateFileW = module.get_import("CreateFileW");
         */
        [[nodiscard]] void* get_import(std::string_view symbol_name) const noexcept {
            if (!m_loaded) return nullptr;

            auto* dos = m_base.cast<IMAGE_DOS_HEADER*>();
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;

            auto* nt = m_base.at_offset(dos->e_lfanew).cast<IMAGE_NT_HEADERS*>();
            if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;

            const auto& import_dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            if (import_dir.Size == 0) return nullptr;

            auto* imports = m_base.at_offset(import_dir.VirtualAddress).cast<IMAGE_IMPORT_DESCRIPTOR*>();

            for (; imports->Name != 0; ++imports) {
                auto* thunk = m_base.at_offset(imports->OriginalFirstThunk).cast<IMAGE_THUNK_DATA*>();
                auto** func = m_base.at_offset(imports->FirstThunk).cast<void**>();

                for (; thunk->u1.AddressOfData != 0; ++thunk, ++func) {
                    if (IMAGE_SNAP_BY_ORDINAL(thunk->u1.Ordinal)) continue;

                    auto* import = m_base.at_offset(thunk->u1.AddressOfData).cast<IMAGE_IMPORT_BY_NAME*>();
                    if (symbol_name == import->Name) {
                        return func;
                    }
                }
            }
            return nullptr;
        }

        /**
         * @brief Check if module was found and loaded
         * @return true if module is loaded, false otherwise
         */
        [[nodiscard]] bool loaded() const noexcept {
            return m_loaded;
        }

        /**
         * @brief Wait for module to be loaded into memory
         * @param timeout_ms Max milliseconds to wait (0 = wait forever)
         * @return true if module loaded before timeout, false otherwise
         * @example
         * // Wait up to 5 seconds
         * module.wait_for_module(5000);
         * // Wait indefinitely
         * module.wait_for_module();
         */
        bool wait_for_module(unsigned long timeout_ms = 0) noexcept {
            const unsigned long start = GetTickCount64();

            while (!try_get_module()) {
                if (timeout_ms && (GetTickCount64() - start >= timeout_ms)) {
                    break;
                }
                Sleep(50);
            }
            return m_loaded;
        }

        /**
         * @brief Get the module name (without path)
         * @return Wide string view of the module name
         */
        [[nodiscard]] std::wstring_view name() const noexcept {
            return m_base_name;
        }

        /**
         * @brief Get the full module path
         * @return Wide string view of the full module path
         */
        [[nodiscard]] std::wstring_view full_path() const noexcept {
            return m_full_path;
        }

    private:
        /**
         * @brief Attempt to find module in PEB loader list
         * @return true if module found, false otherwise
         */
        bool try_get_module() noexcept {
            if (m_loaded) return true;

            auto* teb = NtCurrentTeb();
            if (!teb) return false;

            auto* peb = reinterpret_cast<PEB*>(teb->ProcessEnvironmentBlock);
            if (!peb || !peb->Ldr) return false;

            // If no name specified, get the main module
            if (m_base_name.empty()) {
                auto* entry = peb->Ldr->InMemoryOrderModuleList.Flink;
                auto* module = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);

                m_full_path = std::wstring_view(module->FullDllName.Buffer,
                                              module->FullDllName.Length / sizeof(wchar_t));
                // For base name, we'll extract it from the full path
                auto last_slash = m_full_path.find_last_of(L'\\');
                if (last_slash != std::wstring_view::npos) {
                    m_base_name = std::wstring(m_full_path.substr(last_slash + 1));
                } else {
                    m_base_name = std::wstring(m_full_path);
                }
                
                m_base = address_t(module->DllBase);
                m_loaded = true;

                auto* dos = m_base.cast<IMAGE_DOS_HEADER*>();
                auto* nt = m_base.at_offset(dos->e_lfanew).cast<IMAGE_NT_HEADERS*>();
                m_size = nt->OptionalHeader.SizeOfImage;
                return true;
            }

            // Search for the named module
            auto* head = &peb->Ldr->InMemoryOrderModuleList;
            for (auto* entry = head->Flink; entry != head; entry = entry->Flink) {
                auto* module = CONTAINING_RECORD(entry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
                
                std::wstring_view full_path(module->FullDllName.Buffer,
                                          module->FullDllName.Length / sizeof(wchar_t));
                
                // Extract base name from full path
                auto last_slash = full_path.find_last_of(L'\\');
                std::wstring_view current_name = (last_slash != std::wstring_view::npos) 
                    ? full_path.substr(last_slash + 1)
                    : full_path;

                // Case insensitive comparison using base name
                if (std::equal(m_base_name.begin(), m_base_name.end(),
                              current_name.begin(), current_name.end(),
                              [](wchar_t a, wchar_t b) { return towlower(a) == towlower(b); })) {
                    m_full_path = std::wstring(full_path);
                    m_base = address_t(module->DllBase);
                    m_loaded = true;

                    auto* dos = m_base.cast<IMAGE_DOS_HEADER*>();
                    auto* nt = m_base.at_offset(dos->e_lfanew).cast<IMAGE_NT_HEADERS*>();
                    m_size = nt->OptionalHeader.SizeOfImage;
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief Convert narrow string to wide string
         * @param str Narrow string to convert
         * @return Converted wide string
         */
        static std::wstring to_wide(std::string_view str) noexcept {
            if (str.empty()) return std::wstring();
            
            int size_needed = MultiByteToWideChar(CP_UTF8, 0, 
                                                str.data(), static_cast<int>(str.size()),
                                                nullptr, 0);
            std::wstring result(size_needed, 0);
            MultiByteToWideChar(CP_UTF8, 0,
                            str.data(), static_cast<int>(str.size()),
                            result.data(), size_needed);
            return result;
        }

    private:
        std::wstring m_base_name;  ///< Module name without path
        std::wstring m_full_path;  ///< Full module path
        bool m_loaded;            ///< Load state
    };

} // namespace mem