#pragma once 

#include <atomic>
#include <bit>
#include <concepts>
#include <exception>
#include <memory>
#include <optional>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

template <typename T>
    requires std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>
class SPSCQueue final 
{
public:
    explicit SPSCQueue(size_t capacity)
        : capacity_{capacity},
          capacity_mask{capacity - 1},
          buffer{static_cast<T*>(operator new[](capacity * sizeof(T)))}
    {
        if (!std::has_single_bit(capacity)) {
            operator delete[](buffer);
            throw std::invalid_argument("Set capacity to a power of two");
        }
    }

    ~SPSCQueue() {

        size_t h = head.load(std::memory_order_relaxed);
        size_t t = tail.load(std::memory_order_relaxed);
        while (h != t) {
            std::destroy_at(&buffer[h]);
            h = (h + 1) & capacity_mask;
        }
        operator delete[](buffer);
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue(SPSCQueue&&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;
    SPSCQueue& operator=(SPSCQueue&&) = delete;

    template <typename... Args>
    bool try_emplace(Args&&... args)
    {
        const size_t t = tail.load(std::memory_order_relaxed);
        const size_t h = head.load(std::memory_order_acquire);

        if (((t + 1) & capacity_mask) == h) {
            return false;
        }

        std::construct_at(&buffer[t], std::forward<Args>(args)...);
        tail.store((t + 1) & capacity_mask, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> try_pop()
    {
        const size_t h = head.load(std::memory_order_relaxed);
        const size_t t = tail.load(std::memory_order_acquire);

        if (h == t) {
            return std::nullopt;
        }

        T result = std::move(buffer[h]);
        std::destroy_at(&buffer[h]);
        head.store((h + 1) & capacity_mask, std::memory_order_release);
        return result;
    }

private:
    static constexpr size_t cache_line = 64;
    
    const size_t capacity_;
    const size_t capacity_mask;
    T* const buffer;

    alignas(cache_line) std::atomic<size_t> head{0};
    alignas(cache_line) std::atomic<size_t> tail{0};
}; 