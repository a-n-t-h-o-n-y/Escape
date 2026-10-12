#ifndef ESCAPE_EXAMPLES_LUMEN_WORKERS_HPP
#define ESCAPE_EXAMPLES_LUMEN_WORKERS_HPP

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <exception>
#include <functional>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace lumen {
// Reuse a small pool for large frames; the calling thread also renders rows.
class RowWorkers {
    std::vector<std::thread> threads;
    std::mutex mutex;
    std::condition_variable work, done;
    std::function<void(int)> task;
    std::exception_ptr error;
    std::atomic<int> next_row{0};
    int rows = 0;
    unsigned generation = 0, remaining = 0;
    bool stopping = false;

    void process()
    {
        try {
            for (;;) {
                int row = next_row.fetch_add(1, std::memory_order_relaxed);
                if (row >= rows) break;
                task(row);
            }
        }
        catch (...) {
            std::lock_guard lock{mutex};
            if (!error) error = std::current_exception();
        }
    }

    void stop()
    {
        {
            std::lock_guard lock{mutex};
            stopping = true;
        }
        work.notify_all();
        for (auto& thread : threads)
            thread.join();
    }

   public:
    RowWorkers()
    {
        unsigned count = std::clamp(std::thread::hardware_concurrency(), 1u, 8u);
        threads.reserve(count - 1);
        try {
            for (unsigned i = 1; i < count; ++i)
                threads.emplace_back([this] {
                    unsigned seen = 0;
                    std::unique_lock lock{mutex};
                    for (;;) {
                        work.wait(lock, [&] { return stopping || generation != seen; });
                        if (stopping) return;
                        seen = generation;
                        lock.unlock();
                        process();
                        lock.lock();
                        if (--remaining == 0) done.notify_one();
                    }
                });
        }
        catch (...) {
            stop();
            throw;
        }
    }

    ~RowWorkers() { stop(); }

    void run(int count, std::function<void(int)> render)
    {
        {
            std::lock_guard lock{mutex};
            task = std::move(render);
            rows = count;
            next_row = 0;
            error = nullptr;
            remaining = static_cast<unsigned>(threads.size());
            ++generation;
        }
        work.notify_all();
        process();
        std::unique_lock lock{mutex};
        done.wait(lock, [&] { return remaining == 0; });
        task = {};
        if (error) std::rethrow_exception(error);
    }
};
}  // namespace lumen

#endif
