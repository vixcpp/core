/**
 *
 *  @file RequestState.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#ifndef VIX_REQUEST_STATE_HPP
#define VIX_REQUEST_STATE_HPP

#include <unordered_map>
#include <typeindex>
#include <any>
#include <stdexcept>

namespace vix::vhttp
{
  class RequestState
  {
  public:
    RequestState() : data_{} {}
    RequestState(const RequestState &) = delete;
    RequestState &operator=(const RequestState &) = delete;
    RequestState(RequestState &&) noexcept = default;
    RequestState &operator=(RequestState &&) noexcept = default;

    template <class T, class... Args>
    T &emplace(Args &&...args)
    {
      auto key = std::type_index(typeid(T));
      data_[key] = std::make_any<T>(std::forward<Args>(args)...);
      return std::any_cast<T &>(data_[key]);
    }

    template <class T>
    void set(T value)
    {
      auto key = std::type_index(typeid(T));
      data_[key] = std::make_any<T>(std::move(value));
    }

    template <class T>
    bool has() const noexcept
    {
      return data_.find(std::type_index(typeid(T))) != data_.end();
    }

    template <class T>
    T &get()
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        throw std::runtime_error("Request state not found for type: " + std::string(typeid(T).name()));
      return std::any_cast<T &>(it->second);
    }

    template <class T>
    const T &get() const
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        throw std::runtime_error("Request state not found for type: " + std::string(typeid(T).name()));
      return std::any_cast<const T &>(it->second);
    }

    template <class T>
    T *try_get() noexcept
    {
      auto it = data_.find(std::type_index(typeid(T)));
      if (it == data_.end())
        return nullptr;
      return std::any_cast<T>(&it->second);
    }

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
}

#endif
