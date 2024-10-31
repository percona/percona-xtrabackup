/*
   Copyright (c) 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef CS_MTA_METRICS_AGGREGATOR_H
#define CS_MTA_METRICS_AGGREGATOR_H

#include <atomic>
#include "sql/changestreams/apply/metrics/applier_metrics_interface.h"
#include "sql/changestreams/apply/metrics/time_based_metric.h"

namespace cs::apply::instruments {

/// @brief This class contains metrics related to event and transaction
/// scheduling activities in the replica MTA
class Applier_metrics : public Applier_metrics_interface {
 public:
  /// @brief Starts the timer when the applier metrics collection began.
  /// Sets the state to running.
  /// This can be queried later to know for how long time the stats have been
  /// collected, i.e., the duration.
  void start_applier_timer() override;

  /// @brief Calculates the total time the applier ran.
  /// Sets the state to not running
  /// Sums to time since start to the total running time
  void stop_applier_timer() override;

  /// @brief Gets the time point when the metric timer started.
  /// @return The time point since the collection of statistics started.
  int64_t get_last_applier_start_micros() const override;

  /// @brief Returns the total time the applier was running
  /// @return Amount of time the applier threads were running for this channel
  int64_t get_total_execution_time() const override;

  /// @brief increment the number of transactions committed.
  /// @param amount the amount of transactions to increment.
  void inc_transactions_committed_count(int64_t amount) override;

  /// @brief Gets the number of transactions committed.
  /// @return the number of transactions committed.
  int64_t get_transactions_committed_count() const override;

  /// @brief increment the number of transactions pending.
  /// @param amount the amount of transactions to increment.
  void inc_transactions_received_count(int64_t amount) override;

  /// @brief Gets the number of transactions pending.
  /// @return the number of transactions waiting execution.
  int64_t get_transactions_received_count() const override;

  /// @brief increment the size of transactions committed.
  /// @param amount the size amount to increment.
  void inc_transactions_committed_size_sum(int64_t amount) override;

  /// @brief Gets the total sum of the size of committed transactions
  /// @return the total size of committed transactions
  int64_t get_transactions_committed_size_sum() const override;

  /// @brief increment the pending size of queued transactions.
  /// @param amount the size amount to increment.
  void inc_transactions_received_size_sum(int64_t amount) override;

  /// @brief Gets the pending size sum of queued transactions
  /// @return the exectuted size of pending transactions
  int64_t get_transactions_received_size_sum() const override;

  /// @brief increment the number of events scheduled by a given amount.
  /// @param delta the amount of events to increment.
  void inc_events_committed_count(int64_t delta) override;

  /// @brief Gets the number of events scheduled.
  /// @return the number of events scheduled.
  int64_t get_events_committed_count() const override;

  /// @brief Resets the statistics to zero.
  void reset() override;

  /// @brief Set the metrics breakpoint, if it has not already been set.
  ///
  /// @param relay_log_filename relay log name
  void set_metrics_breakpoint(const char *relay_log_filename) override;

  /// @return true if the coordinator has advanced past the metrics breakpoint.
  bool is_after_metrics_breakpoint() const override;

  /// If we are before the metrics breakpoint, and the metrics breakpoint is
  /// equal to the given filename, remember that we are now after the metrics
  /// breakpoint.
  void check_metrics_breakpoint(const char *relay_log_filename) override;

  /// @brief Returns time metrics for waits on work from the source
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  Time_based_metric_interface &get_work_from_source_wait_metric() override;

  /// @brief Returns time metrics for waits on available workers
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  Time_based_metric_interface &get_workers_available_wait_metric() override;

  /// @brief Returns time metrics for waits on transaction dependecies on
  /// workers
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  Time_based_metric_interface &get_transaction_dependency_wait_metric()
      override;

  /// @brief Returns time metrics for waits when a worker queue exceeds max
  /// memory
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  Time_based_metric_interface &
  get_worker_queues_memory_exceeds_max_wait_metric() override;

  /// @brief Returns time metrics for waits when the worker queues are full
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  Time_based_metric_interface &get_worker_queues_full_wait_metric() override;

  /// @brief Returns time metrics for relay log read wait times
  /// @return a Time_based_metric_interface object that contains metric
  /// information on a wait
  Time_based_metric_interface &get_time_to_read_from_relay_log_metric()
      override;

  /// @brief Increments the stored values for the commit order metrics.
  /// @param count The count for commit order waits
  /// @param time The time waited on commit order
  void inc_commit_order_wait_stored_metrics(int64_t count,
                                            int64_t time) override;

  /// @brief Gets the stored number of times we waited on committed order
  /// @return the stored number of commit order waits
  int64_t get_number_of_waits_on_commit_order() const override;

  /// @brief Gets the stored summed time waited on commit order
  /// @return the stored sum of the time waited on commit
  int64_t get_wait_time_on_commit_order() const override;

 private:
  /// @brief stats collection start timestamp
  std::atomic<int64_t> m_last_applier_start_micros{};

  /// @brief total applier execution time
  Time_based_metric m_sum_applier_execution_time;

  /// @brief Holds the counter of transactions committed.
  std::atomic_int64_t m_transactions_committed{0};

  /// @brief Holds the counter of transactions currently pending.
  std::atomic_int64_t m_transactions_received_count{0};

  /// @brief The first (oldest) relay log that the receiver wrote to after
  /// metric collection was enabled.
  std::string m_first_received_relay_log;

  /// @brief State of the metrics breakpoint
  enum class Metrics_breakpoint_state {
    /// Breakpoint not set yet (nothing received).
    unset,
    /// Breakpoint set by receiver; applier is positioned before it
    before,
    /// Breakpoint set by receiver; applier is positioned after it
    after
  };
  /// @brief True if the receiver has initialized m_first_received_relay_log.
  std::atomic<Metrics_breakpoint_state> m_metrics_breakpoint_state{
      Metrics_breakpoint_state::unset};

  /// @brief Holds the total size of transactions committed till now.
  std::atomic_int64_t m_transactions_committed_size_sum{0};

  /// @brief Holds the executed event's size of transactions now ongoing
  std::atomic_int64_t m_transactions_received_size_sum{0};

  /// @brief Holds the counter of events scheduled.
  std::atomic_int64_t m_events_committed_count{0};

  // wait because there are no transactions to apply ---

  /// @brief Tracks the number and time waited for transactions to apply
  Time_based_metric m_wait_for_work_from_source{true};

  // wait because no workers available ---

  /// @brief Tracks the number and time waited for transactions to apply
  Time_based_metric m_wait_for_worker_available;

  // waits for transaction dependency ---

  /// @brief Tracks the number and time waited for transaction dependencies
  Time_based_metric m_wait_for_transaction_dependency;

  // waits because worker queues memory exceeds max ---

  /// @brief Tracks the number and time waited for transaction dependencies
  Time_based_metric m_wait_due_to_worker_queues_memory_exceeds_max;

  // wait because worker queues are full ---

  /// @brief Tracks the number and time waited for transaction dependencies
  Time_based_metric m_wait_due_to_worker_queue_full;

  // Time spent reading from the relay log ---

  /// @brief Tracks the number and time waited for transaction dependencies
  Time_based_metric m_time_to_read_from_relay_log;

  /// @brief The stored number of time waited for commit order
  std::atomic_int64_t m_order_commit_wait_count{0};

  /// @brief The stored total amount of time waited for commit order
  std::atomic_int64_t m_order_commit_waited_time{0};
};
}  // namespace cs::apply::instruments

#endif /* CS_MTA_METRICS_AGGREGATOR_H */
