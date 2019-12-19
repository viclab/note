#include "timeout_queue.h"

#include <random>
#include <iostream>

#define ERR_LOG(p1,p2,p3,p4,format,...) fprintf(stdout, format, __VA_ARGS__)

uint32_t TimeoutQueue::Add(const Task& task, uint64_t expire_time, uint32_t interval_time)
{
    uint32_t new_id = GenerateID();
    if (timer_id_index_.insert({new_id, expire_time}).second == false)
    {
        ERR_LOG(0, 0, 0, "", "timer_id_index_ insert new_id(%u) error", new_id);
        return 0;
    }

    if (timer_queue_.insert({new_id, interval_time, expire_time, task}).second == false)
    {
        ERR_LOG(0, 0, 0, "", "timer_queue_ insert new_id(%u) error", new_id);
        return 0;
    }
    return new_id;
}

uint32_t TimeoutQueue::Add(Task&& task, uint64_t expire_time, uint32_t interval_time)
{
    uint32_t new_id = GenerateID();
    if (timer_id_index_.insert({new_id, expire_time}).second == false)
    {
        ERR_LOG(0, 0, 0, "", "timer_id_index_ insert new_id(%u) error", new_id);
        return 0;
    }

    if (timer_queue_.insert({new_id, interval_time, expire_time, std::move(task)}).second == false)
    {
        ERR_LOG(0, 0, 0, "", "timer_queue_ insert new_id(%u) error", new_id);
        return 0;
    }

    return new_id;
}

bool TimeoutQueue::Cancel(uint32_t time_id)
{
    auto iter = timer_id_index_.find(time_id);
    if (iter == timer_id_index_.end())
    {
        return false;
    }

    timer_queue_.erase({time_id, 0, iter->second, nullptr});
    timer_id_index_.erase(iter);
    return true;
}

uint32_t TimeoutQueue::TimeOut(uint64_t now)
{
    uint32_t count = 0;
    while (!timer_queue_.empty())
    {
        auto iter = timer_queue_.begin();
        if (iter->expire_time > now)
        {
            break;
        }

        // 拷贝一份出来后先删除定时器，防止task中有逻辑对这个定时器有操作
        Timer tmp = (*iter);
        timer_id_index_.erase(iter->seq_id);
        timer_queue_.erase(iter);

        tmp.task();
        ++count;

        // 是循环定时器，重新加进去
        if (tmp.interval_time > 0)
        {
            tmp.expire_time += tmp.interval_time;

            if (timer_id_index_.insert({tmp.seq_id, tmp.expire_time}).second)
            {
                if (timer_queue_.insert(tmp).second == false)
                {
                    ERR_LOG(0, 0, 0, "", "timer_queue_ insert interval id(%u) error", tmp.seq_id);
                }
            }
            else
            {
                ERR_LOG(0, 0, 0, "", "timer_id_index_ insert interval id(%u) error", tmp.seq_id);
            }
        }
    };

    return count;
}

uint32_t TimeoutQueue::GenerateID()
{
    ++base_id_;
    if (base_id_ == 0)
    {
        ++base_id_;
    }
    return ++base_id_;
}

int main()
{
    TimeoutQueue s_mgr;

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
            s_mgr.Add([&]() { }, range_interval(rand) + now, 0);
        }
        else if (run_time <= 1000)
        {
            if (!t1)
            {
                t1 = s_mgr.TimerSize();
            }
            s_mgr.TimeOut(start + 1000);
            //std::uniform_int_distribution<> range_delid(0, s_mgr.TimerSize());
            //s_mgr.RemoveTimer(range_delid(rand));
        }
        else
        {
            if (!t2)
            {
                t2 = s_mgr.TimerSize();
            }
            std::cout << "[" << n << "]: " << "timer total size: " << t2 << ", add: " << t1 - t2_old << "/500ms, Del: " << t1 - t2 << "/500ms" << std::endl;
            start = now;
            ++n;
            t1_old = t1, t2_old = t2;
            t1 = t2 = 0;
        }
    }

    return 0;
}


