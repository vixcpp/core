/**
 * @file RequestState.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_REQUEST_STATE_HPP
#define VIX_REQUEST_STATE_HPP

#include <any>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

namespace vix::vhttp
{
  /** @brief Type-safe per-request storage using std::any keyed by std::type_index (useful for middleware data sharing). */
  class RequestState
  {
  public:
    /** @brief Create an empty request state container. */
    RequestState() : data_{} {}

    RequestState(const RequestState &) = delete;
    RequestState &operator=(const RequestState &) = delete;

    RequestState(RequestState &&) noexcept = default;
    RequestState &operator=(RequestState &&) noexcept = default;

    /** @brief Construct a value of type T in the state and return a reference to it. */
    template <class T, class... Args>
    T &emplace(Args &&...args)
    {
      const auto key = std::type_index(typeid(T));
      data_[key] = std::make_any<T>(std::forward<Args>(args)...);
      return std::any_cast<T &>(data_[key]);
    }

    /** @brief Store a value of type T in the state (overwrites any previous value for T). */
    template <class T>
    void set(T value)
    {
      const auto key = std::type_index(typeid(T));
      data_[key] = std::make_any<T>(std::move(value));
    }

    /** @brief Return true if a value of type T exists in the state. */
    template <class T>
    bool has() const noexcept
    {
      return data_.find(std::type_index(typeid(T))) != data_.end();
    }

    /** @brief Get a mutable reference to the stored T or throw if missing. */
    template <class T>
    T &get()
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        throw std::runtime_error("RequestState missing type: " + std::string(typeid(T).name()));
      return std::any_cast<T &>(it->second);
    }

    /** @brief Get a const reference to the stored T or throw if missing. */
    template <class T>
    const T &get() const
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        throw std::runtime_error("RequestState missing type: " + std::string(typeid(T).name()));
      return std::any_cast<const T &>(it->second);
    }

    /** @brief Get a pointer to the stored T or nullptr if missing. */
    template <class T>
    T *try_get() noexcept
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        return nullptr;
      return std::any_cast<T>(&it->second);
    }

    /** @brief Get a const pointer to the stored T or nullptr if missing. */
    template <class T>
    const T *try_get() const noexcept
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        return nullptr;
      return std::any_cast<const T>(&it->second);
    }

  private:
    std::unordered_map<std::type_index, std::any> data_;
  };

} // namespace vix::vhttp

#endif // VIX_REQUEST_STATE_HPP
