/**
 *
 * @file Supervisor.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the
 * License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_RUNTIME_SUPERVISOR_HPP
#define VIX_RUNTIME_SUPERVISOR_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace vix::runtime
{

  /**
   * @brief Restart policy for a supervised runtime component.
   */
  enum class RestartPolicy : std::uint8_t
  {
    never = 0,
    always = 1,
    on_failure = 2
  };

  /**
   * @brief Lifecycle state of a supervised component.
   */
  enum class SupervisedState : std::uint8_t
  {
    stopped = 0,
    running = 1,
    failed = 2
  };

  /**
   * @brief One supervised runtime component.
   *
   * A component is identified by name and exposes start/stop hooks.
   * Restart behavior is controlled by a simple policy.
   */
  struct SupervisedComponent
  {
    /** @brief Component name. */
    std::string name;

    /** @brief Start callback. Must return true on success. */
    std::function<bool()> start;

    /** @brief Stop callback. */
    std::function<void()> stop;

    /** @brief Restart policy. */
    RestartPolicy policy;

    /** @brief Current component state. */
    SupervisedState state;

    /**
     * @brief Number of times the component has been restarted.
     */
    std::uint64_t restartCount;

    /**
     * @brief Construct an empty component.
     */
    SupervisedComponent()
        : name(),
          start(),
          stop(),
          policy(RestartPolicy::never),
          state(SupervisedState::stopped),
          restartCount(0)
    {
    }

    /**
     * @brief Construct a supervised component.
     *
     * @param componentName Logical component name.
     * @param startFn Start callback.
     * @param stopFn Stop callback.
     * @param restartPolicy Restart policy.
     */
    SupervisedComponent(std::string componentName,
                        std::function<bool()> startFn,
                        std::function<void()> stopFn,
                        RestartPolicy restartPolicy = RestartPolicy::on_failure)
        : name(std::move(componentName)),
          start(std::move(startFn)),
          stop(std::move(stopFn)),
          policy(restartPolicy),
          state(SupervisedState::stopped),
          restartCount(0)
    {
    }

    /**
     * @brief Check whether the component has valid callbacks.
     *
     * @return true if both start and stop callbacks are present.
     */
    [[nodiscard]] bool valid() const noexcept
    {
      return !name.empty() &&
             static_cast<bool>(start) &&
             static_cast<bool>(stop);
    }
  };

  /**
   * @brief Minimal runtime supervisor for managed components.
   *
   * V1 responsibilities:
   * - register components
   * - start all components
   * - stop all components
   * - restart a component explicitly
   * - expose component state safely
   *
   * Automatic crash detection can be added later on top of this base.
   */
  class Supervisor
  {
  public:
    /**
     * @brief Construct an empty supervisor.
     */
    Supervisor() = default;

    Supervisor(const Supervisor &) = delete;
    Supervisor &operator=(const Supervisor &) = delete;

    Supervisor(Supervisor &&) = delete;
    Supervisor &operator=(Supervisor &&) = delete;

    /**
     * @brief Stop all components on destruction.
     */
    ~Supervisor()
    {
      stop_all();
    }

    /**
     * @brief Register one supervised component.
     *
     * Registration fails if:
     * - the component is invalid
     * - another component already uses the same name
     *
     * @param component Component to register.
     * @return true if registration succeeded.
     */
    [[nodiscard]] bool add(SupervisedComponent component)
    {
      if (!component.valid())
      {
        return false;
      }

      std::lock_guard<std::mutex> lock(mutex_);

      if (find_index_locked(component.name).has_value())
      {
        return false;
      }

      components_.push_back(std::move(component));
      return true;
    }

    /**
     * @brief Return the number of registered components.
     *
     * @return Registered component count.
     */
    [[nodiscard]] std::size_t size() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return components_.size();
    }

    /**
     * @brief Return whether no component is registered.
     *
     * @return true if empty.
     */
    [[nodiscard]] bool empty() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return components_.empty();
    }

    /**
     * @brief Start all registered components in registration order.
     *
     * Components that fail to start are marked as failed.
     *
     * @return true if all components started successfully.
     */
    [[nodiscard]] bool start_all()
    {
      std::lock_guard<std::mutex> lock(mutex_);

      bool ok = true;

      for (auto &component : components_)
      {
        if (component.state == SupervisedState::running)
        {
          continue;
        }

        const bool started = safe_start(component);
        if (!started)
        {
          ok = false;
        }
      }

      return ok;
    }

    /**
     * @brief Stop all running components in reverse registration order.
     */
    void stop_all()
    {
      std::lock_guard<std::mutex> lock(mutex_);

      for (auto it = components_.rbegin(); it != components_.rend(); ++it)
      {
        auto &component = *it;

        if (component.state != SupervisedState::running)
        {
          continue;
        }

        safe_stop(component);
        component.state = SupervisedState::stopped;
      }
    }

    /**
     * @brief Restart one registered component by name.
     *
     * Restart behavior respects the stored policy:
     * - never: restart denied
     * - always: restart allowed
     * - on_failure: restart allowed only if current state is failed
     *
     * @param name Component name.
     * @return true if restart succeeded.
     */
    [[nodiscard]] bool restart(const std::string &name)
    {
      std::lock_guard<std::mutex> lock(mutex_);

      const auto index = find_index_locked(name);
      if (!index.has_value())
      {
        return false;
      }

      auto &component = components_[*index];

      switch (component.policy)
      {
      case RestartPolicy::never:
        return false;

      case RestartPolicy::on_failure:
        if (component.state != SupervisedState::failed)
        {
          return false;
        }
        break;

      case RestartPolicy::always:
      default:
        break;
      }

      if (component.state == SupervisedState::running)
      {
        safe_stop(component);
        component.state = SupervisedState::stopped;
      }

      const bool started = safe_start(component);
      if (started)
      {
        ++component.restartCount;
      }

      return started;
    }

    /**
     * @brief Mark one component as failed.
     *
     * This is useful when another subsystem detects a component failure and
     * wants the supervisor state to reflect it.
     *
     * @param name Component name.
     * @return true if the component was found and marked failed.
     */
    [[nodiscard]] bool mark_failed(const std::string &name)
    {
      std::lock_guard<std::mutex> lock(mutex_);

      const auto index = find_index_locked(name);
      if (!index.has_value())
      {
        return false;
      }

      components_[*index].state = SupervisedState::failed;
      return true;
    }

    /**
     * @brief Return the current state of one component.
     *
     * @param name Component name.
     * @return Component state if found.
     */
    [[nodiscard]] std::optional<SupervisedState> state_of(const std::string &name) const
    {
      std::lock_guard<std::mutex> lock(mutex_);

      const auto index = find_index_locked(name);
      if (!index.has_value())
      {
        return std::nullopt;
      }

      return components_[*index].state;
    }

    /**
     * @brief Return the restart count of one component.
     *
     * @param name Component name.
     * @return Restart count if found.
     */
    [[nodiscard]] std::optional<std::uint64_t> restart_count_of(const std::string &name) const
    {
      std::lock_guard<std::mutex> lock(mutex_);

      const auto index = find_index_locked(name);
      if (!index.has_value())
      {
        return std::nullopt;
      }

      return components_[*index].restartCount;
    }

  private:
    /**
     * @brief Find a component index by name.
     *
     * Caller must already hold the mutex.
     *
     * @param name Component name.
     * @return Component index if found.
     */
    [[nodiscard]] std::optional<std::size_t> find_index_locked(const std::string &name) const
    {
      for (std::size_t i = 0; i < components_.size(); ++i)
      {
        if (components_[i].name == name)
        {
          return i;
        }
      }

      return std::nullopt;
    }

    /**
     * @brief Start one component safely.
     *
     * Caller must already hold the mutex.
     *
     * @param component Component to start.
     * @return true if start succeeded.
     */
    [[nodiscard]] bool safe_start(SupervisedComponent &component)
    {
      try
      {
        const bool started = component.start();
        component.state = started ? SupervisedState::running
                                  : SupervisedState::failed;
        return started;
      }
      catch (...)
      {
        component.state = SupervisedState::failed;
        return false;
      }
    }

    /**
     * @brief Stop one component safely.
     *
     * Caller must already hold the mutex.
     *
     * @param component Component to stop.
     */
    void safe_stop(SupervisedComponent &component) noexcept
    {
      try
      {
        component.stop();
      }
      catch (...)
      {
      }
    }

  private:
    /** @brief Registered supervised components. */
    std::vector<SupervisedComponent> components_;

    /** @brief Mutex protecting supervisor state. */
    mutable std::mutex mutex_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_SUPERVISOR_HPP
