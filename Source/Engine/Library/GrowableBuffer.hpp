#pragma once
#include <cinttypes>
#include <cstddef>

/**
 * @class GrowableBuffer
 * @brief Automatically enlarge the memory when needed
 *
 * A dynamic buffer that grows automatically to accommodate data writes.
 * Supports proper memory alignment for different data types.
 * Uses an exponential growth strategy for efficiency.
 */
class GrowableBuffer
{
public:
    /**
     * @brief Default constructor
     */
    GrowableBuffer();

    /**
     * @brief Destructor - frees allocated memory
     */
    ~GrowableBuffer();

    /**
     * @brief Move constructor
     */
    GrowableBuffer(GrowableBuffer&& other) noexcept;

    /**
     * @brief Move assignment operator
     */
    GrowableBuffer& operator=(GrowableBuffer&& other) noexcept;

    // Disable copy constructor and assignment to prevent accidental copies
    GrowableBuffer(const GrowableBuffer&) = delete;
    GrowableBuffer& operator=(const GrowableBuffer&) = delete;

    /**
     * @brief Write data to the buffer with specified alignment
     * @param data Pointer to the data to write
     * @param size Size of the data in bytes
     * @param alignment Required alignment (must be power of 2)
     */
    size_t Write(void* data, size_t size, size_t alignment);

    /**
     * @brief Write typed data to the buffer with automatic alignment
     * @tparam T Type of data to write
     * @param data Reference to the data to write
     * @param alignment Override alignment (defaults to alignof(T))
     */
    template <class T>
    size_t Write(T& data, size_t alignment = alignof(T))
    {
        return Write(&data, sizeof(T), alignment);
    }

    template <class T>
    T* Read(size_t handle)
    {
        return (T*)((uint8_t*)head + handle);
    }

    /**
     * @brief Reset the buffer (keeps allocated memory but resets size to 0)
     */
    void Reset();

    /**
     * @brief Reserve at least the specified capacity
     * @param capacity Minimum capacity to reserve
     */
    void Reserve(size_t capacity);

    /**
     * @brief Get current size of data in the buffer
     * @return Current size in bytes
     */
    size_t GetSize() const;

    /**
     * @brief Get current capacity of the buffer
     * @return Current capacity in bytes
     */
    size_t GetCapacity() const;

    /**
     * @brief Get pointer to the buffer data
     * @return Pointer to the beginning of the buffer
     */
    void* GetData() const;

    /**
     * @brief Get pointer to the buffer data (same as GetData for compatibility)
     * @return Pointer to the beginning of the buffer
     */
    void* GetHead() { return GetData(); }

private:
    void* head;         ///< Pointer to allocated memory
    size_t totalSize;   ///< Total allocated capacity
    size_t currentSize; ///< Current used size

    /**
     * @brief Ensure the buffer has at least the required capacity
     * @param requiredSize Required minimum size
     */
    void EnsureCapacity(size_t requiredSize);

    /**
     * @brief Grow the buffer to the specified size
     * @param newSize New size to grow to
     */
    void Grow(size_t newSize);

    /**
     * @brief Align a size value to the specified alignment
     * @param size Size to align
     * @param alignment Alignment requirement (must be power of 2)
     * @return Aligned size
     */
    static size_t AlignSize(size_t size, size_t alignment);
};
