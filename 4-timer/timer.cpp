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

int main()
{
    Singleton<TimerMgr> s_mgr;

    std::default_random_engine rand;
    std::uniform_int_distribution<> range_interval(3, 20), range_loop(0, 1);
    for (size_t i = 0; i < 100; i++)
    {
        s_mgr.GetInst().AddTimer(range_interval(rand), time(nullptr), [&](const TimerObj* obj) {
                cout << "TIMEOUT[" << s_mgr.GetInst().TimerSize() << "]:" << "now:" << time(nullptr) << ", timer[" << obj->id << "]: timeout, detail:" << obj->ShortDebugString() << endl;
                }, range_loop(rand), nullptr);
    }

    while (1)
    {
        s_mgr.GetInst().LoopUpdate(time(nullptr));
        usleep(100 * 1000);
    }

    return 0;
}
