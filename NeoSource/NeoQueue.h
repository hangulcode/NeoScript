#pragma once

#include <deque>
#include <algorithm>
#include <mutex>
#include <condition_variable>

namespace NeoScript
{

template<typename T>
class NeoThreadSafeQueue {
private:
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;

public:
    NeoThreadSafeQueue() {}

    void Push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(value);
        cond_.notify_one();
    }

    // 완료 상태와 이벤트를 큐 공개와 함께 갱신한다. 소비자는 잠금 해제 후에만 접근한다.
    template<typename Publish>
    void PushAndPublish(const T& value, Publish publish) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push_back(value);
        publish();
        cond_.notify_one();
    }

    void WaitAndPop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop_front();
    }

    bool TryPop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
        value = queue_.front();
        queue_.pop_front();
        return true;
    }

    template<typename Predicate>
    bool TryPopMatching(T& value, Predicate predicate) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(queue_.begin(), queue_.end(), predicate);
        if (it == queue_.end())
            return false;
        value = *it;
        queue_.erase(it);
        return true;
    }

    bool Empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }
};

class NeoEvent {
public:
    NeoEvent() {}
    ~NeoEvent() {}

    void reset()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _signaled = false;
    }

    /**
     * Signal to event.
     */
    void set()
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _signaled = true;
        }
        _cv.notify_all();
    }
    /**
     * Wait for signal.
     */
    bool wait(int ms)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (ms == -1)
        {
            _cv.wait(lock, [this] { return _signaled; });
            return true;
        }
        return _cv.wait_for(lock, std::chrono::milliseconds(ms), [this] { return _signaled; });
    }
private:
    std::condition_variable _cv;
    std::mutex _mutex;
    bool _signaled = false;
};

};

