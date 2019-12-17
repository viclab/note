#include <cinttypes>
#include <cstdlib>
#include <vector>

#include <iostream>
#include <algorithm>
#include <chrono>
#include <functional>
#include <random>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>

class Timer final {
    using TimerCallback = std::function<void()>;
 public:
    Timer() {};
    ~Timer() {};

    int Init() {

        return 0;
    }
    void AddTimer(uint32_t interval, uint64_t abs_time, TimerCallback cb, bool is_loop, void* extend) {
        TimerObj obj(interval, interval + abs_time, cb, is_loop, extend);
        timer_heap_.push_back(obj);
        heap_up(timer_heap_.size());
    }

    void RemoveTimer(const size_t timer_id) {
        if (timer_id == 0) {
            return;
        }
        size_t now_idx = timer_id - 1;
        if (now_idx >= timer_heap_.size()) {
            return;
        }

        TimerObj obj = timer_heap_[now_idx];
        obj.SetTimerID(0);
        std::swap(timer_heap_[timer_heap_.size() - 1], timer_heap_[now_idx]);
        timer_heap_.pop_back();

        if (timer_heap_.empty()) {
            return;
        }

        if (timer_heap_[now_idx] < obj) {
            heap_up(now_idx + 1);
        } else if (timer_heap_[now_idx] == obj) {
            obj.SetTimerID(now_idx + 1);
        } else {
            heap_down(now_idx);
        }
    }

    void PopTimeout() {
        if (timer_heap_.empty()) {
            return;
        }

        timer_heap_[0].SetTimerID(0);

        std::swap(timer_heap_[0], timer_heap_[timer_heap_.size() - 1]);
        timer_heap_.pop_back();

        if (timer_heap_.size() > 0) {
            heap_down(0);
        }

    }

    int OnUpdate(uint64_t end) {
        int n = 0;
        while (GetNextTimeout() == 0 && GetTimestampMS() <= end) {
            const TimerObj& obj = timer_heap_.front();
            obj.cb_();
            PopTimeout();
            ++n;
        }
    }

    const int GetNextTimeout() const {
        if (timer_heap_.empty()) {
            return -1;
        }

        int next_timeout = 0;
        TimerObj obj = timer_heap_.front();
        uint64_t now_time = GetTimestampMS();
        if (obj.abs_time_ > now_time) {
            next_timeout = (int)(obj.abs_time_ - now_time);
        }
        return next_timeout;
    }

    const bool empty() {
         return timer_heap_.empty();
    }

    static const uint64_t GetTimestampMS() {
        auto now_time = std::chrono::system_clock::now();
        uint64_t now = (std::chrono::duration_cast<std::chrono::milliseconds>(now_time.time_since_epoch())).count();
        return now;
    }

    static const uint64_t GetSteadyClockMS() {
        auto now_time = std::chrono::steady_clock::now();
        uint64_t now = (std::chrono::duration_cast<std::chrono::milliseconds>(now_time.time_since_epoch())).count();
        return now;
    }

    static void MsSleep(const int time_ms) {
        timespec t;
        t.tv_sec = time_ms / 1000;
        t.tv_nsec = (time_ms % 1000) * 1000000;
        int ret = 0;
        do {
            ret = ::nanosleep(&t, &t);
        } while (ret == -1 && errno == EINTR);
    }

    size_t TimerSize() { return timer_heap_.size(); }

    //std::vector<UThreadSocket_t *> GetSocketList() {
    //    std::vector<UThreadSocket_t *> socket_list;
    //    for (auto &obj : timer_heap_) {
    //        socket_list.push_back(obj.socket_);
    //    }
    //    return socket_list;
    //}

 private:
    void heap_up(const size_t end_idx) {
        size_t now_idx = end_idx - 1;
        TimerObj obj = timer_heap_[now_idx];
        size_t parent_idx = (now_idx - 1) / 2;
        while (now_idx > 0 && parent_idx >= 0 && obj < timer_heap_[parent_idx]) {
            timer_heap_[now_idx] = timer_heap_[parent_idx];
            timer_heap_[now_idx].SetTimerID(now_idx + 1);
            now_idx = parent_idx;
            parent_idx = (now_idx - 1) / 2;
        }

        timer_heap_[now_idx] = obj;
        timer_heap_[now_idx].SetTimerID(now_idx + 1);
    }

    void heap_down(const size_t begin_idx) {
        size_t now_idx = begin_idx;
        TimerObj obj = timer_heap_[now_idx];
        size_t child_idx = (now_idx + 1) * 2;
        while (child_idx <= timer_heap_.size()) {
            if (child_idx == timer_heap_.size()
                    || timer_heap_[child_idx - 1] < timer_heap_[child_idx]) {
                child_idx--;
            }

            if (obj < timer_heap_[child_idx]
                    || obj == timer_heap_[child_idx]) {
                break;
            }

            timer_heap_[now_idx] = timer_heap_[child_idx];
            timer_heap_[now_idx].SetTimerID(now_idx + 1);
            now_idx = child_idx;
            child_idx = (now_idx + 1) * 2;
        }

        timer_heap_[now_idx] = obj;
        timer_heap_[now_idx].SetTimerID(now_idx + 1);
    }

    struct TimerObj {
        TimerObj(uint32_t interval, uint64_t abs_time, TimerCallback cb, bool is_loop, void* extend)
                : interval_(interval),
                abs_time_(abs_time),
                cb_(cb),
                extend_(extend) {
        }

        uint32_t id_;
        uint32_t interval_;
        uint64_t abs_time_;
        bool is_loop_;
        TimerCallback cb_;
        void* extend_;

        void SetTimerID(uint32_t timer_id) {
            id_ = timer_id;
        }

        bool operator <(const TimerObj &obj) const {
            return abs_time_ < obj.abs_time_;
        }

        bool operator ==(const TimerObj &obj) const {
            return abs_time_ == obj.abs_time_;
        }
    };

    std::vector<TimerObj> timer_heap_;
};


int main()
{
    Timer s_mgr;
    //bool add = true;
    //uint64_t start = s_mgr.GetTimestampMS();
    //std::cout << "================================================" << start << "=================================================" << std::endl;
    //s_mgr.AddTimer(500, s_mgr.GetTimestampMS(), [&]() {
    //        add = false;
    //        //std::cout << "STOP ADD_TIMER|total timer size:" << s_mgr.TimerSize();<< "" << s_mgr.TimerSize() / (time(nullptr) - start) << "]:now:" << time(nullptr)  << std::endl;
    //        }, false, nullptr);


    std::default_random_engine rand;
    std::uniform_int_distribution<> range_interval(1, 500), range_loop(0, 1);

    int n = 1;
    uint64_t now = 0;
    uint64_t run_time = 0;
    uint64_t t1 = 0, t2 = 0, t1_old = 0, t2_old = 0;
    uint64_t start = s_mgr.GetTimestampMS();
    while (1)
    {
        now = s_mgr.GetTimestampMS();
        run_time = now - start;

        if (run_time <= 500)
        {
            s_mgr.AddTimer(range_interval(rand), now, [&]() {
                    //cout << "TIMEOUT[" << s_mgr.TimerSize() << "]:" << "now:" << time(nullptr) << ", timer[" << obj->id << "]: timeout, detail:" << obj->ShortDebugString() << endl;
                    //std::cout << "TIMEOUT[" << s_mgr.TimerSize() << "|" << s_mgr.TimerSize() / (time(nullptr) - start) << "]:" << std::endl;
                    }, range_loop(rand), nullptr);
        }
        else if (run_time <= 1000)
        {
            if (!t1)
            {
                t1 = s_mgr.TimerSize();
            }
            //s_mgr.OnUpdate(start + 1000);
            std::uniform_int_distribution<> range_delid(0, s_mgr.TimerSize());
            s_mgr.RemoveTimer(range_delid(rand));
        }
        else
        {
            if (!t2)
            {
              t2 = s_mgr.TimerSize();
            }
            std::cout << "[" << n << "]: " << "timer total size: " << t1 << ", add: " << t1 - t2_old << "/500ms, Del: " << t1 - t2 << "/500ms" << std::endl;
            start = now;
            ++n;
            t1_old = t1, t2_old = t2;
            t1 = t2 = 0;
        }
    }

    return 0;
}

