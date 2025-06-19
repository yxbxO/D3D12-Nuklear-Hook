#pragma once
namespace mem
{ 
	/**
	 * @brief A utility class for pattern scanning/signature matching in memory
	 * 
	 * The pattern class provides functionality to create and store byte patterns
	 * used in memory scanning. It supports two formats:
	 * 1. IDA-style signatures (e.g. "48 8B 05 ?? ?? ?? ??")
	 * 2. Raw byte array with mask (e.g. "\x48\x8B\x05\x00\x00\x00\x00", "xxx????")
	 */
	class pattern {
	public:
		/**
		 * @brief Constructs a pattern from an IDA-style signature
		 * @param ida_sig The IDA-style pattern string (e.g. "48 8B 05 ?? ?? ?? ??")
		 */
		pattern(const char* ida_sig) {
			// Count actual pattern length (excluding spaces)
			std::size_t len = 0;
			const char* s = ida_sig;
			while (*s) {
				if (*s == ' ') {
					s++;
					continue;
				}
				if (*s == '?') {
					len++;
					s += (*s && s[1] == '?') ? 2 : 1;
				}
				else {
					len++;
					s += 2;
				}
			}

			// Pre-allocate vectors
			bytes_.resize(len);
			masks_.resize(len);

			// Parse the pattern
			std::size_t i = 0;
			while (*ida_sig) {
				if (*ida_sig == ' ') {
					ida_sig++;
					continue;
				}

				if (*ida_sig == '?') {
					masks_[i] = 0;
					if (ida_sig[1] == '?') ida_sig++;
				}
				else {
					masks_[i] = 1;
					bytes_[i] = (to_hex(*ida_sig) << 4) | to_hex(ida_sig[1]);
				}
				ida_sig += 2;
				i++;
			}
		}

		/**
		 * @brief Constructs a pattern from a byte array and mask
		 * @param data Pointer to the byte array
		 * @param mask String mask where 'x' means match and '?' means wildcard
		 */
		pattern(const void* data, const char* mask) {
			const std::size_t len = std::strlen(mask);
			
			// Pre-allocate vectors
			bytes_.resize(len);
			masks_.resize(len);

			const auto* byte_data = static_cast<const unsigned char*>(data);
			for (std::size_t i = 0; i < len; i++) {
				masks_[i] = (mask[i] != '?');
				bytes_[i] = masks_[i] ? byte_data[i] : 0;
			}
		}

		// Default move operations
		pattern(pattern&&) noexcept = default;
		pattern& operator=(pattern&&) noexcept = default;

		// Default destructor
		~pattern() = default;

		// Prevent copying
		pattern(const pattern&) = delete;
		pattern& operator=(const pattern&) = delete;

		/**
		 * @brief Get the pattern length
		 * @return Number of bytes in the pattern
		 */
		[[nodiscard]] std::size_t size() const noexcept {
			return bytes_.size();
		}

		/**
		 * @brief Get the pattern bytes
		 * @return Pointer to the pattern bytes
		 */
		[[nodiscard]] const unsigned char* get_bytes() const noexcept {
			return bytes_.data();
		}

		/**
		 * @brief Get the pattern masks
		 * @return Pointer to the pattern masks (1 = match, 0 = wildcard)
		 */
		[[nodiscard]] const uint8_t* get_masks() const noexcept {
			return masks_.data();
		}

	private:
		std::vector<unsigned char> bytes_;  ///< Pattern bytes
		std::vector<uint8_t> masks_;       ///< Pattern masks (1 = match, 0 = wildcard)

		/**
		 * @brief Convert a hex character to its numeric value
		 * @param c Hex character ('0'-'9', 'a'-'f', 'A'-'F')
		 * @return Numeric value of the hex character
		 */
		static unsigned char to_hex(char c) noexcept {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return 0;
		}
	};
}