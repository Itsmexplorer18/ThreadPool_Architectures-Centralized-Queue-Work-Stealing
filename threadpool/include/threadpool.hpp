#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <algorithm>

using namespace std;

class ThreadPool {
private:
    int m_threads;
    vector<thread> threads;
    queue<function<void()>> tasks;
    mutex mtx;
    condition_variable cv;
    bool stop;

public:
    explicit ThreadPool(int numthreads) : m_threads{numthreads}, stop{false} {
        for (int i = 0; i < m_threads; i++) {
            threads.emplace_back([this]() {
                while (true) {
                    function<void()> currtask;
                    unique_lock<mutex> lock(mtx);
                    cv.wait(lock, [this] {
                        return !tasks.empty() || stop;
                    });
                    if (stop && tasks.empty()) return;
                    if (tasks.empty()) continue;  
                    currtask = move(tasks.front());
                    tasks.pop();
                    lock.unlock();
                    currtask();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            unique_lock<mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto &th : threads) {
            th.join();
        }
    }

    template<class F, class... Args>
    auto executetasks(F&& f, Args&&... args) -> future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...));
        future<return_type> res = task->get_future();
        {
            unique_lock<mutex> lock(mtx);
            tasks.emplace([task]() {
                (*task)();
            });
        }
        cv.notify_one();
        return res;
    }
};

// QuickSort implementation for ThreadPool
void quicksort(vector<int>& arr, int left, int right) {
    if (left >= right) return;
    
    int pivot = arr[left + (right - left) / 2];
    int i = left, j = right;
    
    while (i <= j) {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;
        
        if (i <= j) {
            swap(arr[i], arr[j]);
            i++;
            j--;
        }
    }
    
    if (left < j) quicksort(arr, left, j);
    if (i < right) quicksort(arr, i, right);
}

vector<int> sortTask(int taskId, int size, const vector<int>& arr) {
    vector<int> copy = arr;
    quicksort(copy, 0, copy.size() - 1);
    return copy;
}

#endif // THREADPOOL_H
