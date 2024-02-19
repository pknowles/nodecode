// Copyright (c) 2024 Pyarelal Knowles, MIT License

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>

#if __has_include(<ranges>)
    #include <ranges>
#endif

namespace nodecode {

template <class Allocator>
concept AllocatorType = requires(Allocator allocator, typename Allocator::value_type) {
    {
        allocator.allocate(std::declval<std::size_t>())
    } -> std::same_as<typename Allocator::value_type*>;
    {
        allocator.deallocate(std::declval<typename Allocator::value_type*>(),
                             std::declval<std::size_t>())
    } -> std::same_as<void>;
};

template <class T>
concept CanReallocate = AllocatorType<T> && requires(T allocator, typename T::value_type) {
    { allocator.reallocate(std::declval<T*>(), std::declval<size_t>()) } -> std::same_as<T*>;
};

template <typename T>
concept TriviallyDestructible = std::is_trivially_destructible_v<T>;

// A possibly-growable local linear arena allocator.
// - growable: The backing allocation may grow if it has reallocate() and the
//   call returns the same address.
// - local: Has per-allocator-instance state
// - linear: Gives monotonic/sequential but aligned allocations that cannot be
//   freed or reused. There is only a reset() call. Only trivially destructible
//   objects should be created from this.
// - arena: Allocations come from a single blob/pool and when it is exhausted
//   std::bad_alloc is thrown (unless a reallocate() is possible).
template <AllocatorType ParentAllocator = std::allocator<std::byte>>
class linear_memory_resource {
public:
    using parent_allocator = ParentAllocator;
    linear_memory_resource() = delete;
    linear_memory_resource(const linear_memory_resource& other) = delete;
    linear_memory_resource(linear_memory_resource&& other) noexcept = delete;
    linear_memory_resource(size_t                 initialSize = 1024 * 1024,
                           const ParentAllocator& parentAllocator = ParentAllocator())
        : m_parentAllocator(parentAllocator)
        , m_begin(m_parentAllocator.allocate(initialSize))
        , m_next(reinterpret_cast<uintptr_t>(m_begin))
        , m_end(reinterpret_cast<uintptr_t>(m_begin) + initialSize) {}
    ~linear_memory_resource() { m_parentAllocator.deallocate(m_begin, bytesAllocated()); }
    linear_memory_resource& operator=(const linear_memory_resource& other) = delete;
    linear_memory_resource& operator=(linear_memory_resource&& other) noexcept = delete;

    [[nodiscard]] constexpr void* allocate(std::size_t bytes, std::size_t align) {
        // Align
        uintptr_t result = m_next + ((-m_next) & (align - 1));

        // Allocate
        m_next = result + bytes;

        // Check for overflow and attempt to reallocate if possible
        if (m_next > m_end) {
            if constexpr (CanReallocate<ParentAllocator>) {
                std::byte* addr = m_parentAllocator.reallocate(m_begin, 2 * bytesReserved());
                if (addr != m_begin) {
                    throw std::bad_alloc();
                }
            } else {
                throw std::bad_alloc();
            }
        }
        return reinterpret_cast<void*>(result);
    }

    constexpr void deallocate(void* p, std::size_t bytes) {
        // Do nothing
        (void)p;
        (void)bytes;
    }

    size_t bytesAllocated() const { return m_next - reinterpret_cast<uintptr_t>(m_begin); }
    size_t bytesReserved() const { return m_end - reinterpret_cast<uintptr_t>(m_begin); }
    void   reset() { m_next = reinterpret_cast<uintptr_t>(m_begin); }

    // Returns a pointer to the arena/parent allocation.
    void* arena() const { return reinterpret_cast<void*>(m_begin); }

private:
    ParentAllocator m_parentAllocator;
    std::byte*      m_begin;
    uintptr_t       m_next;
    uintptr_t       m_end;
};

// STL compatible allocator with an implicit constructor from
// linear_memory_resource. Emphasizes why std::pmr is a thing - ParentAllocator
// shouldn't affect the type.
template <TriviallyDestructible T, class MemoryResource = linear_memory_resource<>>
class linear_allocator {
public:
    using memory_resource = MemoryResource;
    using value_type = T;
    linear_allocator(memory_resource& resource)
        : m_resource(resource) {}

    [[nodiscard]] constexpr T* allocate(std::size_t n) {
        return static_cast<T*>(m_resource.allocate(n * sizeof(T), alignof(T)));
    }

    constexpr void deallocate(T* p, std::size_t n) {
        return m_resource.deallocate(static_cast<void*>(p), n);
    }

private:
    memory_resource& m_resource;
};

template <class T, class MemoryResource, class... Args>
T* create(MemoryResource& memoryResource, Args&&... args) {
    return std::construct_at<T>(linear_allocator<T, MemoryResource>(memoryResource).allocate(1),
                                std::forward<Args>(args)...);
};

template <class T, class MemoryResource>
std::span<T> createArray(MemoryResource& memoryResource, size_t size) {
    auto result =
        std::span(linear_allocator<T, MemoryResource>(memoryResource).allocate(size), size);
    for (auto& obj : result)
        std::construct_at<T>(&obj);
    return result;
};

#ifdef __cpp_lib_ranges
template <std::ranges::input_range Range, class MemoryResource>
std::span<std::ranges::range_value_t<Range>> createArray(MemoryResource& memoryResource,
                                                         const Range&    range) {
    using T = std::ranges::range_value_t<Range>;
    auto size = std::ranges::size(range);
    auto result =
        std::span(linear_allocator<T, MemoryResource>(memoryResource).allocate(size), size);
    auto out = result.begin();
    for (auto& in : range)
        std::construct_at<T>(&*out++, in);
    return result;
};
#endif

} // namespace nodecode
