/**
 *
 * @file run_queue_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/runtime/RunQueue.hpp>
#include <vix/runtime/Task.hpp>

namespace
{
  using RunQueue = vix::runtime::RunQueue;
  using Task = vix::runtime::Task;
  using TaskId = vix::runtime::TaskId;
  using TaskResult = vix::runtime::TaskResult;
  using TaskState = vix::runtime::TaskState;

  static Task make_task(TaskId id, std::uint32_t affinity = 0)
  {
    return Task{
        id,
        []()
        {
          return TaskResult::complete;
        },
        affinity};
  }

  static Task make_yielding_task(TaskId id)
  {
    return Task{
        id,
        []()
        {
          return TaskResult::yield;
        }};
  }

  static Task make_invalid_task(TaskId id = 0)
  {
    return Task{
        id,
        vix::runtime::TaskFn{}};
  }

  static void test_run_queue_type_traits()
  {
    static_assert(std::is_default_constructible_v<RunQueue>);

    static_assert(!std::is_copy_constructible_v<RunQueue>);
    static_assert(!std::is_copy_assignable_v<RunQueue>);

    static_assert(!std::is_move_constructible_v<RunQueue>);
    static_assert(!std::is_move_assignable_v<RunQueue>);

    static_assert(std::is_destructible_v<RunQueue>);
  }

  static void test_default_queue_is_empty()
  {
    RunQueue queue;

    assert(queue.empty());
    assert(queue.size() == 0u);

    assert(!queue.front().has_value());
    assert(!queue.back().has_value());
    assert(!queue.try_pop().has_value());
    assert(!queue.try_steal().has_value());
    assert(queue.try_pop_batch(8).empty());
  }

  static void test_push_accepts_ready_schedulable_task()
  {
    RunQueue queue;

    Task task = make_task(1);

    assert(task.state == TaskState::ready);
    assert(task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(accepted);
    assert(!queue.empty());
    assert(queue.size() == 1u);

    auto front = queue.front();
    auto back = queue.back();

    assert(front.has_value());
    assert(back.has_value());

    assert(front->id == 1u);
    assert(back->id == 1u);
    assert(front->state == TaskState::ready);
    assert(back->state == TaskState::ready);
  }

  static void test_push_normalizes_yielded_task_to_ready()
  {
    RunQueue queue;

    Task task = make_yielding_task(2);
    task.state = TaskState::yielded;

    assert(task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(accepted);
    assert(queue.size() == 1u);

    auto popped = queue.try_pop();

    assert(popped.has_value());
    assert(popped->id == 2u);
    assert(popped->state == TaskState::ready);
    assert(popped->schedulable());
  }

  static void test_push_rejects_invalid_task()
  {
    RunQueue queue;

    Task task = make_invalid_task();

    assert(!task.valid());
    assert(!task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(!accepted);
    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_push_rejects_running_task()
  {
    RunQueue queue;

    Task task = make_task(3);
    task.state = TaskState::running;

    assert(!task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(!accepted);
    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_push_rejects_completed_task()
  {
    RunQueue queue;

    Task task = make_task(4);
    task.state = TaskState::completed;

    assert(task.done());
    assert(!task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(!accepted);
    assert(queue.empty());
  }

  static void test_push_rejects_failed_task()
  {
    RunQueue queue;

    Task task = make_task(5);
    task.state = TaskState::failed;

    assert(task.done());
    assert(!task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(!accepted);
    assert(queue.empty());
  }

  static void test_push_rejects_cancelled_task()
  {
    RunQueue queue;

    Task task = make_task(6);
    task.state = TaskState::cancelled;

    assert(task.done());
    assert(!task.schedulable());

    const bool accepted = queue.push(std::move(task));

    assert(!accepted);
    assert(queue.empty());
  }

  static void test_try_pop_returns_front_lifo_order()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));
    assert(queue.push(make_task(3)));

    assert(queue.size() == 3u);

    auto first = queue.try_pop();
    auto second = queue.try_pop();
    auto third = queue.try_pop();
    auto fourth = queue.try_pop();

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());
    assert(!fourth.has_value());

    assert(first->id == 3u);
    assert(second->id == 2u);
    assert(third->id == 1u);

    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_try_steal_returns_back_oldest_order()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));
    assert(queue.push(make_task(3)));

    assert(queue.size() == 3u);

    auto first = queue.try_steal();
    auto second = queue.try_steal();
    auto third = queue.try_steal();
    auto fourth = queue.try_steal();

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());
    assert(!fourth.has_value());

    assert(first->id == 1u);
    assert(second->id == 2u);
    assert(third->id == 3u);

    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_front_returns_next_local_task_without_removing()
  {
    RunQueue queue;

    assert(queue.push(make_task(10)));
    assert(queue.push(make_task(20)));

    auto front = queue.front();

    assert(front.has_value());
    assert(front->id == 20u);
    assert(queue.size() == 2u);

    auto popped = queue.try_pop();

    assert(popped.has_value());
    assert(popped->id == 20u);
    assert(queue.size() == 1u);
  }

  static void test_back_returns_next_stealable_task_without_removing()
  {
    RunQueue queue;

    assert(queue.push(make_task(10)));
    assert(queue.push(make_task(20)));

    auto back = queue.back();

    assert(back.has_value());
    assert(back->id == 10u);
    assert(queue.size() == 2u);

    auto stolen = queue.try_steal();

    assert(stolen.has_value());
    assert(stolen->id == 10u);
    assert(queue.size() == 1u);
  }

  static void test_front_and_back_same_for_single_task()
  {
    RunQueue queue;

    assert(queue.push(make_task(99)));

    auto front = queue.front();
    auto back = queue.back();

    assert(front.has_value());
    assert(back.has_value());

    assert(front->id == 99u);
    assert(back->id == 99u);

    assert(queue.size() == 1u);
  }

  static void test_try_pop_batch_zero_returns_empty_without_changes()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));

    auto batch = queue.try_pop_batch(0);

    assert(batch.empty());
    assert(queue.size() == 2u);

    auto first = queue.try_pop();
    auto second = queue.try_pop();

    assert(first.has_value());
    assert(second.has_value());

    assert(first->id == 2u);
    assert(second->id == 1u);
  }

  static void test_try_pop_batch_less_than_size()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));
    assert(queue.push(make_task(3)));
    assert(queue.push(make_task(4)));

    auto batch = queue.try_pop_batch(2);

    assert(batch.size() == 2u);
    assert(batch[0].id == 4u);
    assert(batch[1].id == 3u);

    assert(queue.size() == 2u);

    auto next = queue.try_pop();

    assert(next.has_value());
    assert(next->id == 2u);
  }

  static void test_try_pop_batch_equal_to_size()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));
    assert(queue.push(make_task(3)));

    auto batch = queue.try_pop_batch(3);

    assert(batch.size() == 3u);
    assert(batch[0].id == 3u);
    assert(batch[1].id == 2u);
    assert(batch[2].id == 1u);

    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_try_pop_batch_larger_than_size()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));

    auto batch = queue.try_pop_batch(99);

    assert(batch.size() == 2u);
    assert(batch[0].id == 2u);
    assert(batch[1].id == 1u);

    assert(queue.empty());
  }

  static void test_push_batch_accepts_schedulable_tasks()
  {
    RunQueue queue;

    std::vector<Task> tasks;
    tasks.push_back(make_task(1));
    tasks.push_back(make_task(2));
    tasks.push_back(make_task(3));

    const std::size_t accepted = queue.push_batch(std::move(tasks));

    assert(accepted == 3u);
    assert(queue.size() == 3u);

    auto first = queue.try_pop();
    auto second = queue.try_pop();
    auto third = queue.try_pop();

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());

    assert(first->id == 3u);
    assert(second->id == 2u);
    assert(third->id == 1u);
  }

  static void test_push_batch_rejects_unschedulable_tasks()
  {
    RunQueue queue;

    Task running = make_task(2);
    running.state = TaskState::running;

    Task completed = make_task(4);
    completed.state = TaskState::completed;

    std::vector<Task> tasks;
    tasks.push_back(make_task(1));
    tasks.push_back(std::move(running));
    tasks.push_back(make_task(3));
    tasks.push_back(std::move(completed));
    tasks.push_back(make_invalid_task(5));

    const std::size_t accepted = queue.push_batch(std::move(tasks));

    assert(accepted == 2u);
    assert(queue.size() == 2u);

    auto first = queue.try_pop();
    auto second = queue.try_pop();
    auto third = queue.try_pop();

    assert(first.has_value());
    assert(second.has_value());
    assert(!third.has_value());

    assert(first->id == 3u);
    assert(second->id == 1u);
  }

  static void test_push_batch_normalizes_yielded_tasks_to_ready()
  {
    RunQueue queue;

    Task yielded = make_task(1);
    yielded.state = TaskState::yielded;

    std::vector<Task> tasks;
    tasks.push_back(std::move(yielded));

    const std::size_t accepted = queue.push_batch(std::move(tasks));

    assert(accepted == 1u);
    assert(queue.size() == 1u);

    auto popped = queue.try_pop();

    assert(popped.has_value());
    assert(popped->id == 1u);
    assert(popped->state == TaskState::ready);
  }

  static void test_push_batch_empty_vector()
  {
    RunQueue queue;

    std::vector<Task> tasks;

    const std::size_t accepted = queue.push_batch(std::move(tasks));

    assert(accepted == 0u);
    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_clear_empty_queue()
  {
    RunQueue queue;

    assert(queue.empty());

    const std::size_t removed = queue.clear();

    assert(removed == 0u);
    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_clear_returns_removed_count()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));
    assert(queue.push(make_task(3)));

    assert(queue.size() == 3u);

    const std::size_t removed = queue.clear();

    assert(removed == 3u);
    assert(queue.empty());
    assert(queue.size() == 0u);

    assert(!queue.try_pop().has_value());
    assert(!queue.try_steal().has_value());
  }

  static void test_clear_then_reuse_queue()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));

    assert(queue.clear() == 2u);

    assert(queue.empty());

    assert(queue.push(make_task(3)));
    assert(queue.push(make_task(4)));

    assert(queue.size() == 2u);

    auto first = queue.try_pop();
    auto second = queue.try_pop();

    assert(first.has_value());
    assert(second.has_value());

    assert(first->id == 4u);
    assert(second->id == 3u);
  }

  static void test_swap_two_non_empty_queues()
  {
    RunQueue first;
    RunQueue second;

    assert(first.push(make_task(1)));
    assert(first.push(make_task(2)));

    assert(second.push(make_task(10)));
    assert(second.push(make_task(20)));
    assert(second.push(make_task(30)));

    first.swap(second);

    assert(first.size() == 3u);
    assert(second.size() == 2u);

    auto first_a = first.try_pop();
    auto first_b = first.try_pop();
    auto first_c = first.try_pop();

    assert(first_a.has_value());
    assert(first_b.has_value());
    assert(first_c.has_value());

    assert(first_a->id == 30u);
    assert(first_b->id == 20u);
    assert(first_c->id == 10u);

    auto second_a = second.try_pop();
    auto second_b = second.try_pop();

    assert(second_a.has_value());
    assert(second_b.has_value());

    assert(second_a->id == 2u);
    assert(second_b->id == 1u);
  }

  static void test_swap_with_empty_queue()
  {
    RunQueue first;
    RunQueue second;

    assert(first.push(make_task(1)));
    assert(first.push(make_task(2)));

    first.swap(second);

    assert(first.empty());
    assert(second.size() == 2u);

    auto a = second.try_pop();
    auto b = second.try_pop();

    assert(a.has_value());
    assert(b.has_value());

    assert(a->id == 2u);
    assert(b->id == 1u);
  }

  static void test_swap_self_is_noop()
  {
    RunQueue queue;

    assert(queue.push(make_task(1)));
    assert(queue.push(make_task(2)));

    queue.swap(queue);

    assert(queue.size() == 2u);

    auto first = queue.try_pop();
    auto second = queue.try_pop();

    assert(first.has_value());
    assert(second.has_value());

    assert(first->id == 2u);
    assert(second->id == 1u);
  }

  static void test_affinity_is_preserved()
  {
    RunQueue queue;

    assert(queue.push(make_task(1, 7u)));

    auto popped = queue.try_pop();

    assert(popped.has_value());
    assert(popped->id == 1u);
    assert(popped->affinity == 7u);
  }

  static void test_task_callable_is_preserved()
  {
    RunQueue queue;

    int calls = 0;

    Task task{
        1u,
        [&calls]()
        {
          ++calls;
          return TaskResult::complete;
        }};

    assert(queue.push(std::move(task)));

    auto popped = queue.try_pop();

    assert(popped.has_value());
    assert(popped->valid());

    assert(calls == 0);
    assert(popped->run() == TaskResult::complete);
    assert(calls == 1);
  }

  static void test_many_tasks_pop_order()
  {
    RunQueue queue;

    constexpr std::size_t count = 1000u;

    for (std::size_t i = 0; i < count; ++i)
    {
      assert(queue.push(make_task(static_cast<TaskId>(i))));
    }

    assert(queue.size() == count);

    for (std::size_t i = 0; i < count; ++i)
    {
      auto task = queue.try_pop();

      assert(task.has_value());
      assert(task->id == static_cast<TaskId>(count - 1u - i));
    }

    assert(queue.empty());
  }

  static void test_many_tasks_steal_order()
  {
    RunQueue queue;

    constexpr std::size_t count = 1000u;

    for (std::size_t i = 0; i < count; ++i)
    {
      assert(queue.push(make_task(static_cast<TaskId>(i))));
    }

    assert(queue.size() == count);

    for (std::size_t i = 0; i < count; ++i)
    {
      auto task = queue.try_steal();

      assert(task.has_value());
      assert(task->id == static_cast<TaskId>(i));
    }

    assert(queue.empty());
  }

  static void test_concurrent_pushes()
  {
    RunQueue queue;

    constexpr int producer_count = 4;
    constexpr int tasks_per_producer = 250;
    constexpr int total = producer_count * tasks_per_producer;

    std::vector<std::thread> producers;

    for (int p = 0; p < producer_count; ++p)
    {
      producers.emplace_back(
          [&queue, p]()
          {
            const int base = p * tasks_per_producer;

            for (int i = 0; i < tasks_per_producer; ++i)
            {
              const TaskId id = static_cast<TaskId>(base + i);
              assert(queue.push(make_task(id)));
            }
          });
    }

    for (auto &thread : producers)
    {
      thread.join();
    }

    assert(queue.size() == static_cast<std::size_t>(total));
    assert(!queue.empty());

    std::vector<TaskId> ids;
    ids.reserve(static_cast<std::size_t>(total));

    while (auto task = queue.try_pop())
    {
      ids.push_back(task->id);
    }

    assert(ids.size() == static_cast<std::size_t>(total));

    std::sort(ids.begin(), ids.end());

    for (int i = 0; i < total; ++i)
    {
      assert(ids[static_cast<std::size_t>(i)] == static_cast<TaskId>(i));
    }

    assert(queue.empty());
  }

  static void test_concurrent_steals()
  {
    RunQueue queue;

    constexpr int total = 1000;
    constexpr int consumer_count = 4;

    for (int i = 0; i < total; ++i)
    {
      assert(queue.push(make_task(static_cast<TaskId>(i))));
    }

    std::vector<TaskId> ids;
    ids.reserve(static_cast<std::size_t>(total));

    std::mutex ids_mutex;
    std::vector<std::thread> consumers;

    for (int c = 0; c < consumer_count; ++c)
    {
      consumers.emplace_back(
          [&queue, &ids, &ids_mutex]()
          {
            while (true)
            {
              auto task = queue.try_steal();

              if (!task.has_value())
              {
                break;
              }

              std::lock_guard<std::mutex> lock(ids_mutex);
              ids.push_back(task->id);
            }
          });
    }

    for (auto &thread : consumers)
    {
      thread.join();
    }

    assert(ids.size() == static_cast<std::size_t>(total));

    std::sort(ids.begin(), ids.end());

    for (int i = 0; i < total; ++i)
    {
      assert(ids[static_cast<std::size_t>(i)] == static_cast<TaskId>(i));
    }

    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_concurrent_pop_batch()
  {
    RunQueue queue;

    constexpr int total = 1000;
    constexpr int consumer_count = 4;

    for (int i = 0; i < total; ++i)
    {
      assert(queue.push(make_task(static_cast<TaskId>(i))));
    }

    std::vector<TaskId> ids;
    ids.reserve(static_cast<std::size_t>(total));

    std::mutex ids_mutex;
    std::vector<std::thread> consumers;

    for (int c = 0; c < consumer_count; ++c)
    {
      consumers.emplace_back(
          [&queue, &ids, &ids_mutex]()
          {
            while (true)
            {
              auto batch = queue.try_pop_batch(8);

              if (batch.empty())
              {
                break;
              }

              std::lock_guard<std::mutex> lock(ids_mutex);

              for (const auto &task : batch)
              {
                ids.push_back(task.id);
              }
            }
          });
    }

    for (auto &thread : consumers)
    {
      thread.join();
    }

    assert(ids.size() == static_cast<std::size_t>(total));

    std::sort(ids.begin(), ids.end());

    for (int i = 0; i < total; ++i)
    {
      assert(ids[static_cast<std::size_t>(i)] == static_cast<TaskId>(i));
    }

    assert(queue.empty());
    assert(queue.size() == 0u);
  }

  static void test_clear_after_concurrent_pushes()
  {
    RunQueue queue;

    constexpr int producer_count = 4;
    constexpr int tasks_per_producer = 100;

    std::vector<std::thread> producers;

    for (int p = 0; p < producer_count; ++p)
    {
      producers.emplace_back(
          [&queue, p]()
          {
            const int base = p * tasks_per_producer;

            for (int i = 0; i < tasks_per_producer; ++i)
            {
              assert(queue.push(make_task(static_cast<TaskId>(base + i))));
            }
          });
    }

    for (auto &thread : producers)
    {
      thread.join();
    }

    const std::size_t expected =
        static_cast<std::size_t>(producer_count * tasks_per_producer);

    assert(queue.size() == expected);

    const std::size_t removed = queue.clear();

    assert(removed == expected);
    assert(queue.empty());
    assert(queue.size() == 0u);
  }

} // namespace

int main()
{
  test_run_queue_type_traits();

  test_default_queue_is_empty();

  test_push_accepts_ready_schedulable_task();
  test_push_normalizes_yielded_task_to_ready();

  test_push_rejects_invalid_task();
  test_push_rejects_running_task();
  test_push_rejects_completed_task();
  test_push_rejects_failed_task();
  test_push_rejects_cancelled_task();

  test_try_pop_returns_front_lifo_order();
  test_try_steal_returns_back_oldest_order();

  test_front_returns_next_local_task_without_removing();
  test_back_returns_next_stealable_task_without_removing();
  test_front_and_back_same_for_single_task();

  test_try_pop_batch_zero_returns_empty_without_changes();
  test_try_pop_batch_less_than_size();
  test_try_pop_batch_equal_to_size();
  test_try_pop_batch_larger_than_size();

  test_push_batch_accepts_schedulable_tasks();
  test_push_batch_rejects_unschedulable_tasks();
  test_push_batch_normalizes_yielded_tasks_to_ready();
  test_push_batch_empty_vector();

  test_clear_empty_queue();
  test_clear_returns_removed_count();
  test_clear_then_reuse_queue();

  test_swap_two_non_empty_queues();
  test_swap_with_empty_queue();
  test_swap_self_is_noop();

  test_affinity_is_preserved();
  test_task_callable_is_preserved();

  test_many_tasks_pop_order();
  test_many_tasks_steal_order();

  test_concurrent_pushes();
  test_concurrent_steals();
  test_concurrent_pop_batch();
  test_clear_after_concurrent_pushes();

  return 0;
}
