#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace NeoScript
{

template<typename T>
class NeoThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;

public:
    NeoThreadSafeQueue() {}

    void Push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(value);
        cond_.notify_one();
    }

    void WaitAndPop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
    }

    bool TryPop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
        value = queue_.front();
        queue_.pop();
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

    /**
     * Signal to event.
     */
    void set()
    {
        //_cv.notify_one();
        _cv.notify_all();
    }
    /**
     * Wait for signal.
     */
    bool wait(int ms)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        
        if (std::cv_status::no_timeout == _cv.wait_for(lock, std::chrono::milliseconds(ms)))
        {
            // 큐에 값이 들어왔을 경우
            //int value = dataQueue.front();  // 큐의 맨 앞의 값 가져오기
            //dataQueue.pop();  // 큐에서 값 제거
            lock.unlock();  // 뮤텍스 잠금 해제

            // 큐에서 가져온 값 출력
            //std::cout << "값을 가져왔습니다: " << value << std::endl;
            return true;
        }
        else
        {
            // 타임아웃 처리
            //std::cout << "타임아웃되었습니다." << std::endl;
            lock.unlock();
            return false;
        }
    }
private:
    std::condition_variable _cv;
    std::mutex _mutex;
};

};