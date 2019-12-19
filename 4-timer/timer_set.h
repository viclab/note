#ifndef _TIMEOUT_QUEUE_H_
#define _TIMEOUT_QUEUE_H_

#include <functional>
#include <set>
#include <unordered_map>

class TimeoutQueue
{
public:
    using Task = std::function<void()>;

    /// 添加定时器，返回一个唯一的定时器ID，interval_time如果是0则表示只执行一次
    /// 本来打算提供time_out参数的接口的，但是考虑到有可能设置时间偏移，这个类还是不依赖于获取当前时间这种东西吧
    uint32_t Add(const Task& task, uint64_t expire_time, uint32_t interval_time = 0);
    uint32_t Add(Task&& task, uint64_t expire_time, uint32_t interval_time = 0);
    /// 取消定时器
    bool Cancel(uint32_t time_id);
    /// 处理所有比now超时的timer，返回处理了多少个
    uint32_t TimeOut(uint64_t now);

private:
    friend Singleton<TimeoutQueue>;
    TimeoutQueue(const TimeoutQueue&) = delete;
    TimeoutQueue& operator=(const TimeoutQueue&) = delete;

    TimeoutQueue() = default;
    ~TimeoutQueue() = default;

    uint32_t GenerateID();

public:
    struct Timer
    {
        /// 递增的唯一序号
        uint32_t seq_id;
        /// 循环重复定时器间隔时间ms，间隔时间32位够了，也能对齐，两全其美
        uint32_t interval_time;
        /// 到期时间ms
        uint64_t expire_time;
        /// 要执行的任务，话说这个东西有多大？
        Task task;

        friend bool operator<(const Timer& left, const Timer& right)
        {
            if (left.expire_time < right.expire_time)
            {
                return true;
            }
            else if (left.expire_time > right.expire_time)
            {
                return false;
            }
            else if (left.seq_id < right.seq_id)
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    };

private:
    // 本来是打算std::priority_queue来实现，但是这个定时器主要是用来做rpc超时用的，
    // 会频繁取消定时器，在这种场景下make_heap的效率实在令人担忧，还是用set吧
    std::set<Timer> timer_queue_;
    /// seq id到expire_time的映射
    /// 本来如果返回的time id是struct的话，用map就可以了，但是因为循环定时器的存在导致了expire_time是会变的，GG
    std::unordered_map<uint32_t, uint64_t> timer_id_index_;
    /// time id 累加器
    uint32_t base_id_ = 0;
};

#endif
