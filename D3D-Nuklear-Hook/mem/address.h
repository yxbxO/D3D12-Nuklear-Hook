#pragma once
namespace mem {
/// A template class for handling memory addresses with configurable pointer types.
/// Useful for scenarios where you need different pointer sizes than your architecture,
/// such as handling 64-bit addresses in 32-bit programs.
template <typename ptr_type = uintptr_t>
struct address_base_t {
    /// The underlying pointer value
    ptr_type m_ptr;

    /// Creates a null address object
    address_base_t() : m_ptr{} {};

    /// Creates an address object from a raw pointer value
    /// @param ptr The address value to initialize with
    address_base_t(ptr_type ptr) : m_ptr(ptr) {};

    /// Creates an address object from a typed pointer
    /// @param ptr Pointer to convert to an address
    address_base_t(ptr_type* ptr) : m_ptr(ptr_type(ptr)) {};

    /// Creates an address object from a void pointer
    /// @param ptr Generic pointer to convert to an address
    address_base_t(void* ptr) : m_ptr(ptr_type(ptr)) {};

    /// Creates an address object from a const void pointer
    /// @param ptr Const generic pointer to convert to an address
    address_base_t(const void* ptr) : m_ptr(ptr_type(ptr)) {};

    /// Default destructor
    ~address_base_t() = default;

    /// Implicit conversion to underlying pointer type
    inline operator ptr_type() const {
        return m_ptr;
    }

    /// Implicit conversion to void pointer
    inline operator void*() {
        return reinterpret_cast<void*>(m_ptr);
    }

    /// Gets the raw pointer value
    /// @return The underlying pointer value
    inline ptr_type get_inner() const {
        return m_ptr;
    }

    /// Compares this address with another
    /// @param in Address to compare with
    /// @return True if addresses match
    template <typename t = address_base_t<ptr_type>>
    inline bool compare(t in) const {
        return m_ptr == ptr_type(in);
    }

    /// Dereferences the pointer and updates internal value
    /// @param in Number of dereference operations to perform
    /// @return Reference to this object
    inline address_base_t<ptr_type>& self_get(uint8_t in = 1) {
        m_ptr = get<ptr_type>(in);
        return *this;
    }

    /// Adds an offset to the pointer and updates internal value
    /// @param offset The offset to add
    /// @return Reference to this object
    inline address_base_t<ptr_type>& self_offset(ptrdiff_t offset) {
        m_ptr += offset;
        return *this;
    }

    /// Follows a JMP instruction and updates internal value
    /// @param offset Offset to the JMP instruction
    /// @return Reference to this object
    template <typename t = address_base_t<ptr_type>>
    inline address_base_t<ptr_type>& self_jmp(ptrdiff_t offset = 0x1) {
        m_ptr = jmp(offset);
        return *this;
    }

    /// Finds a specific opcode and updates internal value
    /// @param opcode The opcode to search for
    /// @param offset Additional offset to add to result
    /// @return Reference to this object
    inline address_base_t<ptr_type>& self_find_opcode(uint8_t opcode, ptrdiff_t offset = 0x0) {
        m_ptr = find_opcode(opcode, offset);
        return *this;
    }

    /// Finds a sequence of opcodes and updates internal value
    /// @param opcodes Vector of opcodes to search for
    /// @param offset Additional offset to add to result
    /// @return Reference to this object
    inline address_base_t<ptr_type>& self_find_opcode_seq(std::vector<uint8_t> opcodes,
                                                          ptrdiff_t offset = 0x0) {
        m_ptr = find_opcode_seq(opcodes, offset);
        return *this;
    }

    /// Sets the internal pointer to a new value
    /// @param in New address value
    /// @return Reference to this object
    template <typename t = address_base_t<ptr_type>>
    inline address_base_t<ptr_type>& set(t in) {
        m_ptr = ptr_type(in);
        return *this;
    }

    /// Casts the pointer to a different type
    /// @return The casted value
    template <typename t = ptr_type>
    inline t cast() const {
        return t(m_ptr);
    }

    /// Dereferences the pointer multiple times
    /// @param in Number of dereference operations
    /// @return New address object with dereferenced value
    template <typename t = address_base_t<ptr_type>>
    inline t get(uint8_t in = 1) {
        ptr_type dummy = m_ptr;
        while (in--)
            if (dummy)
                dummy = *reinterpret_cast<ptr_type*>(dummy);
        return t(dummy);
    }

    /// Creates a new address offset from this one
    /// @param offset The offset to add
    /// @return New address object at the offset
    template <typename t = address_base_t<ptr_type>>
    [[nodiscard]] inline t at_offset(ptrdiff_t offset) const noexcept {
        return t(m_ptr + offset);
    }

    /// Follows a relative JMP instruction
    /// @param offset Offset to the JMP instruction
    /// @return New address object pointing to the jump target
    /// @note Handles E9 ? ? ? ? style JMP instructions
    template <typename t = address_base_t<ptr_type>>
    inline t jmp(ptrdiff_t offset = 0x1) {
        ptr_type base = m_ptr + offset;
        auto displacement = *reinterpret_cast<int32_t*>(base);
        base += sizeof(uint32_t);  // Skip displacement bytes
        base += displacement;      // Add relative offset
        return t(base);
    }

    /// Searches for a specific opcode
    /// @param opcode The opcode to find
    /// @param offset Additional offset to add to result
    /// @return New address object pointing to found opcode
    template <typename t = address_base_t<ptr_type>>
    inline t find_opcode(uint8_t opcode, ptrdiff_t offset = 0x0) {
        auto base = m_ptr;
        uint8_t opcode_at_address = 0x0;
        while (opcode_at_address = *reinterpret_cast<uint8_t*>(base)) {
            if (opcode == opcode_at_address)
                break;
            base += 1;
        }
        base += offset;
        return t(base);
    }

    /// Searches for a sequence of opcodes
    /// @param opcodes Vector of opcodes to find in sequence
    /// @param offset Additional offset to add to result
    /// @return New address object pointing to start of found sequence
    template <typename t = address_base_t<ptr_type>>
    inline t find_opcode_seq(std::vector<uint8_t> opcodes, ptrdiff_t offset = 0x0) {
        auto base = m_ptr;
        uint8_t opcode_at_address = 0x0;
        while (opcode_at_address = *reinterpret_cast<uint8_t*>(base)) {
            if (opcodes.at(0) == opcode_at_address) {
                for (auto i = 0u; i < opcodes.size(); i++)
                    if (opcodes.at(i) != *reinterpret_cast<uint8_t*>(base + i))
                        goto CONT;
                break;
            }
        CONT:
            base += 1;
        }
        base += offset;
        return t(base);
    }

    /// Reads a value of specified type from the address
    /// @tparam t Type of value to read
    /// @return Value read from memory
    template <typename t>
    inline t read() const {
        return *reinterpret_cast<t*>(m_ptr);
    }

    /// Writes a value of specified type to the address
    /// @tparam t Type of value to write
    /// @param value The value to write
    /// @return True if write succeeded, false if address is null
    template <typename t>
    inline bool write(const t& value) {
        if (!m_ptr)
            return false;
        *reinterpret_cast<t*>(m_ptr) = value;
        return true;
    }

    /// Checks if the address is valid (non-null)
    /// @return True if address is non-null
    inline bool is_valid() const {
        return m_ptr != 0;
    }

    /// Safely dereferences the pointer with null checks
    /// @param depth Number of dereference operations
    /// @return Optional containing the dereferenced value, or nullopt if invalid
    template <typename t = ptr_type>
    inline std::optional<t> safe_deref(uint8_t depth = 1) const {
        ptr_type current = m_ptr;
        try {
            while (depth-- && current) {
                current = *reinterpret_cast<ptr_type*>(current);
            }
            return current ? std::optional<t>(static_cast<t>(current)) : std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    /// Safely reads a value with null check and exception handling
    /// @tparam t Type of value to read
    /// @return Optional containing the read value, or nullopt if failed
    template <typename t>
    inline std::optional<t> safe_read() const {
        try {
            if (!is_valid())
                return std::nullopt;
            return *reinterpret_cast<t*>(m_ptr);
        } catch (...) {
            return std::nullopt;
        }
    }

    /// Comparison operators
    inline bool operator==(const address_base_t& other) const {
        return m_ptr == other.m_ptr;
    }
    inline bool operator!=(const address_base_t& other) const {
        return m_ptr != other.m_ptr;
    }
    inline bool operator<(const address_base_t& other) const {
        return m_ptr < other.m_ptr;
    }
    inline bool operator>(const address_base_t& other) const {
        return m_ptr > other.m_ptr;
    }
    inline bool operator<=(const address_base_t& other) const {
        return m_ptr <= other.m_ptr;
    }
    inline bool operator>=(const address_base_t& other) const {
        return m_ptr >= other.m_ptr;
    }
};

/// Type alias for native pointer size
using address_t = address_base_t<uintptr_t>;

/// Type alias for 32-bit addresses
using address_32_t = address_base_t<uint32_t>;

/// Type alias for 64-bit addresses
using address_64_t = address_base_t<uint64_t>;
}  // namespace mem