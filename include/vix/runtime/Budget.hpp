/**
 *
 * @file Budget.hpp
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
#ifndef VIX_RUNTIME_BUDGET_HPP
#define VIX_RUNTIME_BUDGET_HPP

#include <cstddef>
#include <cstdint>

namespace vix::runtime
{

  /**
   * @brief Runtime execution quantum configuration.
   *
   * A budget defines how much work a task may perform during one scheduling
   * slice before it should yield and be rescheduled.
   *
   * V1 intentionally keeps this generic and lightweight.
   * The unit is "steps", not time.
   */
  struct BudgetConfig
  {
    /**
     * @brief Maximum number of execution steps allowed per scheduling slice.
     */
    std::uint32_t quantum;

    /**
     * @brief Construct a budget configuration.
     *
     * @param q Maximum number of steps allowed per slice.
     */
    explicit BudgetConfig(std::uint32_t q = 64) noexcept
        : quantum(normalize_quantum(q))
    {
    }

    /**
     * @brief Normalize a requested budget quantum.
     *
     * A zero quantum is never allowed because it would make all tasks yield
     * immediately without doing any useful work.
     *
     * @param q Requested quantum.
     * @return Normalized quantum value.
     */
    [[nodiscard]] static constexpr std::uint32_t normalize_quantum(std::uint32_t q) noexcept
    {
      return (q == 0u) ? 1u : q;
    }
  };

  /**
   * @brief Mutable execution budget used while running a task.
   *
   * This object tracks how much work has been consumed during the current
   * scheduling slice.
   */
  class Budget
  {
  public:
    /**
     * @brief Construct a runtime budget from a configuration.
     *
     * @param config Budget configuration.
     */
    explicit Budget(const BudgetConfig &config = BudgetConfig{}) noexcept
        : limit_(BudgetConfig::normalize_quantum(config.quantum)),
          used_(0)
    {
    }

    /**
     * @brief Reset the consumed work for a new scheduling slice.
     */
    void reset() noexcept
    {
      used_ = 0;
    }

    /**
     * @brief Reconfigure the budget limit and reset the consumed work.
     *
     * @param config New budget configuration.
     */
    void reconfigure(const BudgetConfig &config) noexcept
    {
      limit_ = BudgetConfig::normalize_quantum(config.quantum);
      used_ = 0;
    }

    /**
     * @brief Consume one unit of work.
     *
     * @return true if the budget still has capacity after consumption,
     * false if it is exhausted.
     */
    [[nodiscard]] bool consume() noexcept
    {
      if (used_ < limit_)
      {
        ++used_;
      }

      return !exhausted();
    }

    /**
     * @brief Consume several units of work.
     *
     * Consumption is saturating: if @p steps exceeds the remaining capacity,
     * the budget becomes fully exhausted.
     *
     * @param steps Number of steps to consume.
     * @return true if the budget still has capacity after consumption,
     * false if it is exhausted.
     */
    [[nodiscard]] bool consume(std::uint32_t steps) noexcept
    {
      if (steps == 0u)
      {
        return !exhausted();
      }

      if (steps >= remaining())
      {
        used_ = limit_;
        return false;
      }

      used_ += steps;
      return !exhausted();
    }

    /**
     * @brief Force the budget into the exhausted state.
     */
    void exhaust() noexcept
    {
      used_ = limit_;
    }

    /**
     * @brief Check whether the budget is exhausted.
     *
     * @return true if no more work should be executed in this slice.
     */
    [[nodiscard]] bool exhausted() const noexcept
    {
      return used_ >= limit_;
    }

    /**
     * @brief Check whether the budget still has available capacity.
     *
     * @return true if more work may still be executed in this slice.
     */
    [[nodiscard]] bool available() const noexcept
    {
      return !exhausted();
    }

    /**
     * @brief Check whether the budget is empty.
     *
     * @return true if no step has been consumed yet.
     */
    [[nodiscard]] bool empty() const noexcept
    {
      return used_ == 0u;
    }

    /**
     * @brief Check whether the budget is completely full.
     *
     * @return true if the full budget was consumed.
     */
    [[nodiscard]] bool full() const noexcept
    {
      return exhausted();
    }

    /**
     * @brief Check whether the task should yield now.
     *
     * @return true if the task should be rescheduled.
     */
    [[nodiscard]] bool should_yield() const noexcept
    {
      return exhausted();
    }

    /**
     * @brief Return the total budget limit for one slice.
     *
     * @return Maximum number of steps allowed.
     */
    [[nodiscard]] std::uint32_t limit() const noexcept
    {
      return limit_;
    }

    /**
     * @brief Return the amount of work already consumed.
     *
     * @return Number of consumed steps.
     */
    [[nodiscard]] std::uint32_t used() const noexcept
    {
      return used_;
    }

    /**
     * @brief Return the amount of work still available.
     *
     * @return Remaining steps available in the current slice.
     */
    [[nodiscard]] std::uint32_t remaining() const noexcept
    {
      return (used_ >= limit_) ? 0u : (limit_ - used_);
    }

  private:
    /** @brief Maximum number of steps allowed in the current slice. */
    std::uint32_t limit_;

    /** @brief Number of steps already consumed in the current slice. */
    std::uint32_t used_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_BUDGET_HPP
