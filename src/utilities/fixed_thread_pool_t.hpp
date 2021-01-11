#ifndef FIXED_THREAD_POOL_T_H
#define FIXED_THREAD_POOL_T_H

#include <thread>
#include <condition_variable>
#include <future>
#include <functional>
#include <queue>

class fixed_thread_pool_t
{
public:
    explicit fixed_thread_pool_t(size_t thread_count)
        : _data(std::make_shared<data_t>()),
          _size(thread_count)
    {
        for (; thread_count; --thread_count)
            std::thread([data = _data] {
                std::unique_lock<std::mutex> lk(data->mutex);
                while (true)
                {
                    if (!data->tasks.empty())
                    {
                        auto current = std::move(data->tasks.front());
                        data->tasks.pop();
                        lk.unlock();
                        current();
                        lk.lock();
                    }
                    else if (data->is_shutdown)
                        break;
                    else
                        data->cv.wait(lk);
                }
            }).detach();
    }

    fixed_thread_pool_t(fixed_thread_pool_t &&) noexcept = default;

    ~fixed_thread_pool_t()
    {
        if (!_data)
            return;
        {
            std::lock_guard<std::mutex> lk(_data->mutex);
            _data->is_shutdown = true;
        }
        _data->cv.notify_all();
    }

    size_t size() const
    {
        return _size;
    }

    template <class F>
    void launch(F &&task)
    {
        {
            std::lock_guard<std::mutex> lk(_data->mutex);
            _data->tasks.emplace(std::forward<F>(task));
        }
        _data->cv.notify_one();
    }

    template <class F>
    auto submit(F &&f) -> std::future<decltype(f())>
    {
        auto task = std::make_shared<std::packaged_task<decltype(f())()>>(std::forward<F>(f));
        {
            std::lock_guard<std::mutex> lk(_data->mutex);
            _data->tasks.emplace([task] { (*task)(); });
        }
        _data->cv.notify_one();
        return task->get_future();
    }

private:
    struct data_t
    {
        std::mutex mutex;
        std::condition_variable cv;
        std::queue<std::function<void()>> tasks;

        bool is_shutdown = false;
    };
    std::shared_ptr<data_t> _data;
    size_t _size;
};

#endif // FIXED_THREAD_POOL_T_H