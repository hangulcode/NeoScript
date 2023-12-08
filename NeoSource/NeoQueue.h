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
            // ť�� ���� ������ ���
            //int value = dataQueue.front();  // ť�� �� ���� �� ��������
            //dataQueue.pop();  // ť���� �� ����
            lock.unlock();  // ���ؽ� ��� ����

            // ť���� ������ �� ���
            //std::cout << "���� �����Խ��ϴ�: " << value << std::endl;
            return true;
        }
        else
        {
            // Ÿ�Ӿƿ� ó��
            //std::cout << "Ÿ�Ӿƿ��Ǿ����ϴ�." << std::endl;
            lock.unlock();
            return false;
        }
    }
private:
    std::condition_variable _cv;
    std::mutex _mutex;
};

};