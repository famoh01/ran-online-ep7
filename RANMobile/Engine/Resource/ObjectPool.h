#pragma once

#include <vector>
#include <memory>

namespace RANMobile {

template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(size_t initialCapacity = 32) {
        m_pool.reserve(initialCapacity);
        for (size_t i = 0; i < initialCapacity; ++i) {
            m_pool.push_back(std::make_unique<T>());
        }
    }

    std::unique_ptr<T> Acquire() {
        if (m_pool.empty()) {
            return std::make_unique<T>();
        }
        auto obj = std::move(m_pool.back());
        m_pool.pop_back();
        return obj;
    }

    void Release(std::unique_ptr<T> obj) {
        if (obj) {
            m_pool.push_back(std::move(obj));
        }
    }

    size_t AvailableCount() const { return m_pool.size(); }

private:
    std::vector<std::unique_ptr<T>> m_pool;
};

} // namespace RANMobile
