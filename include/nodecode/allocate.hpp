// Copyright (c) 2024 Pyarelal Knowles, MIT License

#pragma once

#include <new>
#include <span>
#include <cstdint>
#include <memory>

#if __has_include(<ranges>)
#include <ranges>
#endif

namespace nodecode {

template <size_t Align> uintptr_t alignUp(uintptr_t ptr) {
    return ptr + ((-ptr) & (Align - 1));
}

namespace uninitialized {

// Tiny linear allocator. There is no deallocator because this is for writing to
// a file or some other destination outside of this address space.
template <size_t Align> uintptr_t alignedAllocate(uintptr_t& ptr, size_t size) {
    uintptr_t result = alignUp<Align>(ptr);
    ptr += size;
    return result;
};

template <size_t Align>
uint8_t* alignedAllocate(std::span<uint8_t>& space, size_t size) {
    uintptr_t result = alignUp<Align>(reinterpret_cast<uintptr_t>(space.data()));
    uintptr_t end = result + size;
    size_t used = end - reinterpret_cast<uintptr_t>(space.data());
    if (used > space.size())
        throw std::bad_alloc();
    space = {reinterpret_cast<uint8_t*>(end), space.size() - used};
    return reinterpret_cast<uint8_t*>(result);
};

template <class T> T *alignedAllocate(std::span<uint8_t> &space) {
    return reinterpret_cast<T *>(
        alignedAllocate<alignof(T)>(space, sizeof(T)));
};

template <class T>
std::span<T> alignedAllocate(std::span<uint8_t> &space, size_t size) {
    auto ptr = reinterpret_cast<T *>(
        alignedAllocate<alignof(T)>(space, sizeof(T) * size));
    return std::span(ptr, size);
};

}; // namespace uninitialized

template <class T>
T* createLeaked(std::span<uint8_t>& space) {
    auto ptr =
        reinterpret_cast<T*>(uninitialized::alignedAllocate<alignof(T)>(space, sizeof(T)));
    return std::construct_at<T>(ptr);
};

template <class T, class... Args>
T* emplaceLeaked(std::span<uint8_t>& space, Args&&... args) {
    auto ptr =
        reinterpret_cast<T*>(uninitialized::alignedAllocate<alignof(T)>(space, sizeof(T)));
    return std::construct_at<T>(ptr, std::forward<Args>(args)...);
};

template <class T>
std::span<T> createLeaked(std::span<uint8_t>& space, size_t size) {
    auto ptr = reinterpret_cast<T*>(
        uninitialized::alignedAllocate<alignof(T)>(space, sizeof(T) * size));
    auto result = std::span(ptr, size);
    for (auto& obj : result)
        std::construct_at<T>(&obj);
    return result;
};

#ifdef __cpp_lib_ranges
template <class Range>
std::span<std::ranges::range_value_t<Range>> emplaceRangeLeaked(std::span<uint8_t>& space, const Range& range) {
    auto size = std::ranges::size(range);
    using T = std::ranges::range_value_t<Range>;
    auto ptr = reinterpret_cast<T*>(
        uninitialized::alignedAllocate<alignof(T)>(space, sizeof(T) * size));
    auto result = std::span(ptr, size);
    auto out = result.begin();
    for (auto& in : range)
        std::construct_at<T>(&*out++, in);
    return result;
};
#endif

} // namespace nodecode
