#include <iostream>
#include <sstream>
#include <queue>
#include <random>
#include <functional>
#include <unistd.h>
using namespace std;

struct TimerObj;

struct TimerObj
{
    int id;
    bool is_loop;
    int interval;
    time_t expire_time;
    std::function<void(const TimerObj*)> func;
    void* extend;
    bool operator < (const TimerObj& r) const
    {
        return expire_time > r.expire_time;
    };
    string ShortDebugString() const
    {
        stringstream ss;
        ss << "id: " << id
            << ", interval:" << interval
            << ", expire_time:" << expire_time
            //<< ", extend:" << extend
            << ", is_loop:" << is_loop;

        return ss.str();
    }
};

class TimerMgr
{
using TimerFunc = std::function<void(const TimerObj*)>;

public:
    int AddTimer(int interval, time_t now, TimerFunc func, bool is_loop, void* extend)
    {
        int id = GenerateID();
        timer_queue_.push({id, is_loop, interval, now + interval, func, extend});

        cout << "ADDTIMER:" << "now:" << time(nullptr) << ", timer[" << id << "]:interval:" << interval << endl;

        return id;
    }
    int AddTimer(TimerObj&& obj)
    {
        timer_queue_.push(obj);
        return 0;
    }
    int DelTimer(int id)
    {
        //TODO:
        return 0;
    }

    size_t TimerSize()
    {
        return timer_queue_.size();
    }

public:
    int LoopUpdate(time_t now)
    {
        int n = 0;
        while (!timer_queue_.empty())
        {
            auto&& top = timer_queue_.top();
            if (top.expire_time > now)
            {
                break;
            }

            top.func(&top);
                
            timer_queue_.pop();

            if (top.is_loop)
            {
                TimerObj tmp = top;
                tmp.expire_time = now + top.interval;
                AddTimer(std::move(tmp));
            }
            ++n;
        }

        return n;
    }

private:
    int GenerateID()
    {
        ++id_generater_;
        if (id_generater_ == 0)
        {
            ++id_generater_;
        }

        return id_generater_;
    }
private:
    int id_generater_;
    std::priority_queue<TimerObj> timer_queue_;
};

template<typename T>
class Singleton
{
public:
    static T& GetInst()
    {
        static T instance_;
        return instance_;
    }
};
