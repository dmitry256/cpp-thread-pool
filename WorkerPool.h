#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

class WorkerPool {
public:
  WorkerPool(const uint32_t workers = std::max(1, std::thread::hardware_concurrency() - 1))
      : processing_(true) {
    for (uint32_t i = 1; i <= workers; i++) {
      std::thread t([this]() { process_(); });
      worker_pool_.push_back(move(t));
    }
  }

  WorkerPool(const WorkerPool &) = delete;

  ~WorkerPool() {
    processing_ = false;
    cv_.notify_all();
    for (auto &worker : worker_pool_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

  template <typename F, typename... Args> void submit(F f, Args... args) {
    std::unique_lock<std::mutex> l(m_);
    task_queue_.push(
        std::move([f, args...]() { f(std::forward<Args>(args)...); }));
    cv_.notify_one();
  }

private:
  std::queue<std::function<void()>> task_queue_;
  std::vector<std::thread> worker_pool_;
  std::mutex m_;
  std::condition_variable cv_;
  std::atomic_bool processing_;
  void process_() {
    while (processing_ or !task_queue_.empty()) {
      std::unique_lock<std::mutex> l(m_);
      cv_.wait(l, [this]() { return !task_queue_.empty() or !processing_; });
      if (!task_queue_.empty()) {
        auto t = task_queue_.front();
        task_queue_.pop();
        l.unlock();
        t();
      }
    }
  }
};
