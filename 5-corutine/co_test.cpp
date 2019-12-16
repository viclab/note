#include <iostream>                                                                                                                          
#include <set>                                                                                                                          
#include <list>                                                                                                                          
#include <vector>                                                                                                                          
#include <functional>                                                                                                                          
#include <ucontext.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
#include <cassert>
#include <sys/epoll.h>
#include <cstdint>
#include "timer.h"

using namespace std;

using CoFunc = std::function<void()>;
using timer_mgr = Singleton<TimerMgr>;

class CoMgr
{
    class CoTask
    {
    public:
        friend CoMgr;
        static const uint32_t STACK_SIZE = 64 * 1024;

        uint32_t GetID()
        {
            return id_;
        }

        bool Resume()
        {   
            assert(CoMgr::Instance().GetCurCo() == nullptr);
            CoMgr::Instance().SetCurCo(this);
            swapcontext(GetMainCo(), &context_);
            return true;
        }   

        bool Yield()
        {
            assert(CoMgr::Instance().GetCurCo() != nullptr);
            CoMgr::Instance().SetCurCo(nullptr);
            swapcontext(&context_, GetMainCo());
            return true;
        }

        ucontext_t* GetMainCo()
        {
            return CoMgr::Instance().MainCo();
        }

    private:
        CoTask(CoFunc func)
        {
            Make(func, true);
        }
        ~CoTask()
        {
            delete (char*)context_.uc_stack.ss_sp;
        }

        void Make(CoFunc func, bool is_new)
        {
            func_ = func;
            if (is_new)
            {
                id_ = CoMgr::Instance().GenID();
                getcontext(&context_);
                context_.uc_stack.ss_sp = calloc(1, STACK_SIZE);
                context_.uc_stack.ss_size = STACK_SIZE;
                context_.uc_stack.ss_flags = 0;
                context_.uc_link = GetMainCo();

                makecontext(&context_, reinterpret_cast<void (*)()>(&CoTask::TaskLoop), 0);
            }
        }

        void TaskLoop()
        {
            while (true)
            {
                func_();

                CoMgr::Instance().RecycleCo(this);

                Yield();
            }
        }

    private:
        ucontext_t context_;
        CoFunc func_;
        int id_;
    };


public:
    CoTask* Create(CoFunc func)
    {
        CoTask* co = nullptr;
        if (!free_list_.empty())
        {
            co = free_list_.front();
            free_list_.pop_front();
            co->Make(func, false);
        }
        else
        {
            co = new CoTask(func);
        }

        CoMgr::Instance().UsedCo(co);
        co->Resume();

        return co;
    }

    size_t GetRunningCoNum()
    {
        return used_list_.size();
    }

    void SetCurCo(CoTask* co)
    {
        cur_co_ = co;
    }

    CoTask* GetCurCo()
    {
        return cur_co_;
    }

    static ucontext_t* MainCo()
    {
        static thread_local ucontext_t main_context;
        return &main_context;
    }
    
    static CoMgr& Instance()
    {   
        static CoMgr instance;
        return instance;
    }
    void UsedCo(CoTask* co)
    {
        used_list_.insert(co);
    }
    void RecycleCo(CoTask* co)
    {
        used_list_.erase(co);
        free_list_.push_back(co);
    }

    void AddSock(int fd)
    {
        io_list_.insert(fd); 
    }

    void DelSock(int fd)
    {
        io_list_.erase(fd); 
    }

    const std::set<int>& GetSockList()
    {
        return io_list_;
    }

    int GenID()
    {
        ++id_generater_;
        if (id_generater_ == 0)
        {
            ++id_generater_;
        }

        return id_generater_;
    }

private:
    std::set<CoTask*> used_list_;
    std::list<CoTask*> free_list_;
    std::set<int> io_list_;
    uint32_t id_generater_;
    CoTask* cur_co_ = nullptr;
};

class EchoSvr
{
public:
    static const uint32_t MAX_EVENTS = 10;

    int Init(const char* ip, int port)
    {
        listen_fd_ = Listen(ip, port);

        epollfd_ = epoll_create(10);
        if (epollfd_ < 0)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|epoll create err" << endl;
            return -1;
        }

        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = listen_fd_;
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, listen_fd_, &ev) == -1)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|epoll_ctl: listen_fd_" << endl;
            return -1;
        }

        return 0;
    }

    bool SetNonBlock(int fd, bool flag)
    {
        int ret = 0;

        int tmp = fcntl(fd, F_GETFL, 0);

        if (flag) {
            ret = fcntl(fd, F_SETFL, tmp | O_NONBLOCK);
        } else {
            ret = fcntl(fd, F_SETFL, tmp & (~O_NONBLOCK));
        }

        if (0 != ret) {
            cerr << __FILE__ << ":" << __LINE__ << "|SetBlock fail, err" << strerror(errno) << endl;
        }

        return 0 == ret;
    }

    int Listen(const char* ip, int port)
    {
        int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_fd < 0)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|socket err" << endl;
            return -1;
        }

        int flags = 1;
        if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (char*) &flags, sizeof(flags)) < 0)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|failed to set setsock to reuseaddr" << endl;
        }

        SetNonBlock(listen_fd, true);

        struct sockaddr_in addr;
        bzero(&addr, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        if ('\0' != *ip)
        {
            addr.sin_addr.s_addr = inet_addr(ip);
            if (INADDR_NONE == addr.sin_addr.s_addr)
            {
                cerr << __FILE__ << ":" << __LINE__ << "|failed to convert to inet_addr" << endl;
                return -1;
            }
        }

        if (bind(listen_fd, (struct sockaddr*) &addr, sizeof(addr)) < 0)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|bind failed, err" << strerror(errno) << endl;
            return -1;
        }

        if (listen(listen_fd, 1024) < 0)
        {
            cerr << __FILE__ << ":" << __LINE__ << "listen failed, err" << strerror(errno) << endl;
            return -1;
        }

        return listen_fd;
    }

    void HandleAccept(int listen_fd)
    {
        struct sockaddr addr;
        size_t len = sizeof(addr);
        int fd = accept(listen_fd, &addr, (socklen_t*)&len);
        if (fd <= 0)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|accept err" << endl;
            return;
        }

        auto co = CoMgr::Instance().GetCurCo();
        cout << "co[" << CoMgr::Instance().GetRunningCoNum() << "|" << (co ? co->GetID() : 0) << "]:new conn " << fd << endl;
        SetNonBlock(fd, true);

        struct epoll_event ev;
        ev.events = EPOLLIN | EPOLLET;
        ev.data.fd = fd;
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) == -1)
        {
            cerr << __FILE__ << ":" << __LINE__ << "|epoll_ctl err: conn_sock:" << fd << endl;
            return;
        }

        CoMgr::Instance().AddSock(fd);
    }

    void HandleIO(int fd)
    {
        auto co = CoMgr::Instance().GetCurCo();
        char buf[1024];
        bzero(buf, sizeof(buf));
        // ET模式，尽可能的处理完所有数据
        while (true)
        {
            int ret = recv(fd, buf, 1024, MSG_NOSIGNAL);
            if (ret < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
            {
                cerr << __FILE__ << ":" << __LINE__ << "|fd:" << fd << " recv err(" << errno <<"):" << strerror(errno) << endl;
                close(fd);
                CoMgr::Instance().DelSock(fd);
                break;
            }
            else if (ret == 0)
            {
                cout << "co[" << CoMgr::Instance().GetRunningCoNum() << "|" <<  (co ? co->GetID() : 0) << "]:fd:" << fd << " connect close" << endl;
                close(fd);
                CoMgr::Instance().DelSock(fd);
                break;
            }
            else if (ret > 0)
            {
                cout << "co[" << CoMgr::Instance().GetRunningCoNum() << "|" << (co ? co->GetID() : 0) << "]:fd:" << fd << " recv data|" << buf << endl;
                int ret2 = send(fd, buf, ret, 0);
                if (ret2 <= 0)
                {
                    cerr << __FILE__ << ":" << __LINE__ << "|send err" << endl;
                }

                // 设置1秒超时，方便观察异步切出后创建新协程
                // 如果所有的func都是无阻塞同步的，则始终只有一个协程在运行
                timer_mgr::GetInst().AddTimer(1, time(nullptr), [&](const TimerObj* obj) {
                        cout << "TIMEOUT[" << timer_mgr::GetInst().TimerSize() << "]:" << "now:" << time(nullptr) << ", timer[" << obj->id << "]: timeout, detail:" << obj->ShortDebugString() << endl;
                        auto sco = (decltype(co))obj->extend;
                        // 1s超时后恢复协程
                        sco->Resume();
                        }, false, co);
                // 切出协程
                co->Yield();
            }
        }

    }

    int OnUpdate()
    {
        struct epoll_event events[MAX_EVENTS];
        //epoll_wait第四个参数timeout设置为0则立即返回，设置为-1永久阻塞，>0则为超时时间
        int nfds = epoll_wait(epollfd_, events, MAX_EVENTS, 100); 
        if (nfds <= 0)
        {
            //cerr << __FILE__ << ":" << __LINE__ << "|epoll_wait:" << nfds << endl;
            return -1;
        }

        for (int i = 0; i < nfds; ++i)
        {
            if (events[i].data.fd == listen_fd_)
            {
                CoMgr::Instance().Create(std::bind(&EchoSvr::HandleAccept, this, listen_fd_));

            }
            else
            {
                int fd = events[i].data.fd;
                CoMgr::Instance().Create(std::bind(&EchoSvr::HandleIO, this, fd));
            }
        }

        // 定时器更新逻辑
        timer_mgr::GetInst().LoopUpdate(time(nullptr));

        return 0;
    }

private:
    int listen_fd_;
    int epollfd_;
};


int main()
{
    EchoSvr svr;
    if (svr.Init("0.0.0.0", 8000) != 0)
    {
        return -1;
    }

    while(1)
    {
        svr.OnUpdate();
    }

    return 0;

}
