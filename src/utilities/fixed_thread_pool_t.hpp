#ifndef FIXED_THREAD_POOL_T_H
#define FIXED_THREAD_POOL_T_H

#include <thread>
#include <condition_variable>
#include <functional>
#include <queue>

class fixed_thread_pool_t
{
public:
    explicit fixed_thread_pool_t(size_t thread_count)
        : _data(std::make_shared<data_t>())
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

    fixed_thread_pool_t() = default;
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

    template <class F>
    void execute(F &&task)
    {
        {
            std::lock_guard<std::mutex> lk(data_->mutex);
            _data->tasks.emplace(std::forward<F>(task));
        }
        _data->cv.notify_one();
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
};

#endif // FIXED_THREAD_POOL_T_H