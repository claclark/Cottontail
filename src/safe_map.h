#ifndef COTTONTAIL_SRC_SAFEMAP_H_
#define COTTONTAIL_SRC_SAFEMAP_H_
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <utility>

namespace cottontail {

template <class K, class V, class Compare = std::less<K>> class SafeMap {
public:
  SafeMap() = default;

  SafeMap(const SafeMap &) = delete;
  SafeMap &operator=(const SafeMap &) = delete;
  SafeMap(const SafeMap &&) = delete;
  SafeMap &operator=(const SafeMap &&) = delete;

  bool contains(const K &key) const {
    std::lock_guard<std::mutex> lock(mu_);
    return m_.find(key) != m_.end();
  };

  void set(K key, V value) {
    std::lock_guard<std::mutex> lock(mu_);
    m_[std::move(key)] = std::move(value);
  };

  bool try_get(const K &key, V *out) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = m_.find(key);
    if (it == m_.end())
      return false;
    if (out)
      *out = it->second;
    return true;
  };

  private:
    mutable std::mutex mu_;
    std::map<K, V, Compare> m_;
  };
} // namespace cottontail
#endif // COTTONTAIL_SRC_SAFEMAP_H_
