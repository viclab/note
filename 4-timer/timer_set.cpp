#include <cinttypes>
#include <cstdlib>
#include <set>
#include <unordered_map>

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

class Timer2 final {
    using TimerCallback = std::function<void()>;
 public:
    Timer2() {};
    ~Timer2() {};

    int GenID() {
        ++id_;
        if (id_ == 0)
        {
            ++id_;
        }
        return id_;
    }
    void AddTimer(uint32_t interval, uint64_t abs_time, TimerCallback cb, bool is_loop, void* extend) {
        uint32_t id = GenID();
        id_index_.insert({id, interval + abs_time});
        timer_set_.insert({id, interval, interval + abs_time, is_loop, cb, extend});
    }

    void RemoveTimer(const uint32_t timer_id) {
        auto it = id_index_.find(timer_id);
        if (it == id_index_.end()) {
            return;
        }

        timer_set_.erase({timer_id, 0, it->second, false, nullptr, nullptr});
        
    }

    void PopTimeout() {
        if (timer_set_.empty()) {
            return;
        }

        id_index_.erase(timer_set_.begin()->id_);
        timer_set_.erase(timer_set_.begin());
    }

    int OnUpdate(uint64_t end) {
        int n = 0;
        while (GetNextTimeout() == 0 && GetTimestampMS() <= end) {
            auto it = timer_set_.begin();
            it->cb_();
            PopTimeout();
            ++n;
        }
    }

    const int GetNextTimeout() const {
        if (timer_set_.empty()) {
            return -1;
        }

        int next_timeout = 0;
        auto it = timer_set_.begin();
        uint64_t now_time = GetTimestampMS();
        if (it->abs_time_ > now_time) {
            next_timeout = (int)(it->abs_time_ - now_time);
        }
        return next_timeout;
    }

    const bool empty() {
         return timer_set_.empty();
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

    size_t TimerSize() { return timer_set_.size(); }
    uint32_t GetMaxTimerID() { return id_; }

 private:
    struct TimerObj {
        uint32_t id_;
        uint32_t interval_;
        uint64_t abs_time_;
        bool is_loop_;
        TimerCallback cb_;
        void* extend_;

        void SetTimerID(uint32_t timer_id) {
            id_ = timer_id;
        }

        friend bool operator <(const TimerObj& l, const TimerObj& r) {
            if (l.abs_time_ == r.abs_time_)
            {
                return l.abs_time_ < r.abs_time_;
            }
            return l.abs_time_ < r.abs_time_;
        }
    };

    std::unordered_map<uint32_t, uint64_t> id_index_;
    std::set<TimerObj> timer_set_;
    uint32_t id_ = 0;
};


int main()
{
    Timer2 s_mgr;

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
                    //std::cout << "TIMEOUT[" << s_mgr.TimerSize() << "]:" << "now:" << time(nullptr) << std::endl;
                    //std::cout << "TIMEOUT[" << s_mgr.TimerSize() << "|" << s_mgr.TimerSize() / (time(nullptr) - start) << "]:" << std::endl;
                    }, range_loop(rand), nullptr);
        }
        else if (run_time <= 1000)
        {
            if (!t1)
            {
                t1 = s_mgr.TimerSize();
            }
            s_mgr.OnUpdate(start + 1000);
            //std::uniform_int_distribution<> range_delid(0, s_mgr.TimerSize());
            //s_mgr.RemoveTimer(range_delid(rand));
        }
        else
        {
            if (!t2)
            {
                t2 = s_mgr.TimerSize();
            }
            std::cout << "[" << n << "]: " << "timer total size: " << t1 << ", add: " << t1 - t2_old << "/500ms, Del: " << t1 - t2 << "/500ms, max_id:" <<  s_mgr.GetMaxTimerID() << std::endl;
            start = now;
            ++n;
            t1_old = t1, t2_old = t2;
            t1 = t2 = 0;
        }
    }

    return 0;
}

