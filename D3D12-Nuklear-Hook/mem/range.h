#pragma once
namespace mem {

	//forward declaration
	//extern class handle;
	//extern class pattern;

	enum class CBRET : uint8_t {
		CONTINUE = 0x1,
		RETURN = 0x2
	};

	/**
	 * @brief A class for managing and scanning memory ranges
	 * 
	 * The range class provides functionality to scan memory regions for patterns,
	 * check memory boundaries, and perform memory-related operations within a specified range.
	 * It's particularly useful for pattern scanning and memory manipulation in Windows processes.
	 */
	class range {
	public:
		/**
		 * @brief Constructs a memory range
		 * @param base Starting address of the range
		 * @param size Size of the range in bytes
		 */
		range(address_t base, std::size_t size) noexcept
			: m_base(base)
			, m_size(size) {
		}

		/**
		 * @brief Get the starting address of the range
		 * @return Starting address
		 */
		[[nodiscard]] address_t begin() const noexcept {
			return m_base;
		}

		/**
		 * @brief Get the ending address of the range
		 * @return End address (base + size)
		 */
		[[nodiscard]] address_t end() const noexcept {
			return m_base.at_offset(m_size);
		}

		/**
		 * @brief Get the size of the range
		 * @return Size in bytes
		 */
		[[nodiscard]] std::size_t size() const noexcept {
			return m_size;
		}

		/**
		 * @brief Check if an address is within this range
		 * @param addr Address to check
		 * @return true if address is within range, false otherwise
		 */
		[[nodiscard]] bool contains(address_t addr) const noexcept {
			return addr >= begin() && addr <= end();
		}

		/**
		 * @brief Scan for first occurrence of a pattern
		 * @param sig Pattern to scan for
		 * @return Address of the first match, or null address if not found
		 */
		[[nodiscard]] address_t scan(const pattern& sig) const noexcept {
			MEMORY_BASIC_INFORMATION page_info{};
			const auto scan_end = m_base.at_offset(m_size);

			for (auto current_page = m_base.cast<const BYTE*>();
				 current_page < scan_end.cast<const BYTE*>();
				 current_page = reinterpret_cast<const BYTE*>(reinterpret_cast<uintptr_t>(page_info.BaseAddress) + page_info.RegionSize)) {

				if (!is_page_valid(page_info, current_page)) {
					continue;
				}

				const auto* region_end = static_cast<const BYTE*>(page_info.BaseAddress) + page_info.RegionSize;
				for (auto i = static_cast<const BYTE*>(page_info.BaseAddress);
					 i < region_end - sig.size(); ++i) {
					if (pattern_matches(i, sig.get_bytes(), sig.get_masks(), sig.size())) {
						return address_t(i);
					}
				}
			}
			return address_t{};
		}

		/**
		 * @brief Scan for all occurrences of a pattern
		 * @param sig Pattern to scan for
		 * @return Vector of addresses where pattern was found
		 */
		[[nodiscard]] std::vector<address_t> scan_all(const pattern& sig) const {
			std::vector<address_t> results;
			results.reserve(1024);  // Pre-allocate to avoid reallocations

			MEMORY_BASIC_INFORMATION page_info{};
			const auto scan_end = m_base.at_offset(m_size);

			for (auto current_page = m_base.cast<const BYTE*>();
				 current_page < scan_end.cast<const BYTE*>();
				 current_page = reinterpret_cast<const BYTE*>(reinterpret_cast<uintptr_t>(page_info.BaseAddress) + page_info.RegionSize)) {

				if (!is_page_valid(page_info, current_page)) {
					continue;
				}

				scan_page(static_cast<const BYTE*>(page_info.BaseAddress), 
						 page_info.RegionSize, 
						 sig, 
						 results, 
						 false);
			}
			return results;
		}

		/**
		 * @brief Scan a specific sub-range for a pattern
		 * @param sig Pattern to scan for
		 * @param offset Offset from base address to start scanning
		 * @param scan_range Size of the range to scan
		 * @return Address of the first match, or null address if not found
		 */
		[[nodiscard]] address_t scan_range(const pattern& sig, std::size_t offset, std::size_t scan_range) const noexcept {
			// Create a temporary range object for the scan
			range temp_range(m_base.at_offset(offset), scan_range);
			return temp_range.scan(sig);
		}
		//void scan_all(pattern const& sig, vector<address_t>& results);
	private:
		/**
		 * @brief Check if a memory page is valid and accessible
		 * @param page_info Output parameter for page information
		 * @param current_page Page address to check
		 * @return true if page is valid and accessible, false otherwise
		 */
		[[nodiscard]] static bool is_page_valid(MEMORY_BASIC_INFORMATION& page_info, const BYTE* current_page) noexcept {
			if (VirtualQuery(current_page, &page_info, sizeof(page_info)) == 0) {
				return false;
			}

			return page_info.State == MEM_COMMIT &&
				   !(page_info.Protect & (PAGE_NOACCESS | PAGE_GUARD | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY));
		}

		/**
		 * @brief Check if pattern matches at a specific location
		 * @param target Location to check
		 * @param bytes Pattern bytes
		 * @param masks Pattern masks
		 * @param length Pattern length
		 * @return true if pattern matches, false otherwise
		 */
		[[nodiscard]] static bool pattern_matches(const uint8_t* target, const unsigned char* bytes, const uint8_t* masks, std::size_t length) noexcept {
			for (std::size_t i = 0; i < length; i++) {
				if (masks[i] && bytes[i] != target[i]) {
					return false;
				}
			}
			return true;
		}

		/**
		 * @brief Scan a single memory page for pattern matches
		 * @param base Base address of the page
		 * @param region_size Size of the region to scan
		 * @param sig Pattern to scan for
		 * @param results Vector to store results
		 * @param first_match_only Stop after first match if true
		 */
		static void scan_page(const BYTE* base, std::size_t region_size, const pattern& sig, 
							 std::vector<address_t>& results, bool first_match_only) noexcept {
			const auto* region_end = base + region_size;
			for (auto i = base; i < region_end - sig.size(); ++i) {
				if (pattern_matches(i, sig.get_bytes(), sig.get_masks(), sig.size())) {
					results.emplace_back(i);
					if (first_match_only) {
						return;
					}
				}
			}
		}
		//vector<address_t> do_scan(const pattern& sig, bool first_match_only);
	protected:
		address_t m_base;    ///< Base address of the range
		std::size_t m_size;  ///< Size of the range in bytes
	};

}