
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>

#include "uthreads.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <queue>
#include <memory>
#include <map>
#include <list>




class thread{
public:
    enum place {Ready, Running, Blocked };
    thread_entry_point entry_point_;
    int id_;
    place place_;
    int quantoms;
    char stack[STACK_SIZE];
    thread(thread_entry_point entryPoint, int id,place place) :
            entry_point_(entryPoint), id_(id), place_(place), quantoms(1) {
    }

};

void timer_handler(int);

std::priority_queue<int, std::vector<int>, std::greater<int> > free_id_list;
std::map<int,thread *> all_threads;
std::vector<int> blocked ;
std::vector<int> ready_list;
int running = -1;
sigjmp_buf env[MAX_THREAD_NUM];
int quantum;


typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0){return -1;}

    quantum = quantum_usecs;
    for (int i = 1; i < MAX_THREAD_NUM + 1; ++i) {
        free_id_list.push(i);
    }
    return 0;
}

int uthread_spawn(thread_entry_point entry_point) {
    if (free_id_list.empty()) {
        return -1;
    }
    int free_id = free_id_list.top();
    free_id_list.pop();
    auto thread_ = new thread(entry_point, free_id,thread::Ready);
    ready_list.push_back(free_id);

    address_t sp = (address_t) thread_->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;

    sigsetjmp(env[free_id], 1);
    (env[free_id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[free_id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[free_id]->__saved_mask);
    // todo need add signals l
    all_threads.insert(std::pair<int, thread *>(free_id, thread_));
    return 0;
}

void remove_from_vector(int tid){
    thread::place cur_place = all_threads.at(tid)->place_;
    switch (cur_place) {
        case thread::Ready:
            ready_list.erase(std::remove(ready_list.begin(), ready_list.end(), tid), ready_list.end());
            return;
        case thread::Blocked:
            blocked.erase(std::remove(ready_list.begin(), ready_list.end(), tid), ready_list.end());
            return;
        case thread::Running:
//            todo do this.
            running = -1;
            return;
        default:
            return;
    }
}
int uthread_terminate(int tid) {
    int res;
    std::map<int, thread *>::iterator thread_iter;
    thread_iter = all_threads.find(tid);

    if (thread_iter == all_threads.end()) {
        return -1;
    }
    all_threads.erase(thread_iter);
    remove_from_vector(tid);
    return 0;
}

int uthread_block(int tid) {
    if (tid == 0 || all_threads.count(tid) == 0) {
        return -1;
    }
    auto cur = all_threads.at(tid);
    auto place = cur->place_;
    if (place == thread::Blocked) {
        return 0;
    } else{
        remove_from_vector(tid);
        blocked.push_back(tid);
    }
    cur->place_ = thread::Blocked;
    if (running == tid){
        sigsetjmp(env[tid], 1);
        ready_list.push_back(tid);
        int now_running = ready_list.at(0   );
        running = now_running;
        all_threads.at(now_running)->place_ = thread::Running;
        siglongjmp(env[now_running], 1);
    }
    return 0;
}

int uthread_resume(int tid) {
    if (all_threads.count(tid) == 0) {
        return -1;
    }
    auto cur = all_threads.at(tid);
    auto place = cur->place_;
    if (place == thread::Ready || place == thread::Running) {
        return 0;
    } else {
        ready_list.push_back(tid);
        cur->place_ = thread::Ready;
        return 0;
    }

}
int uthread_sleep(int num_quantums){
    int result = sigsetjmp(env[running], 1);
    if (result == 0){
        all_threads[running]->place_ = thread::Blocked;
        int now_running = ready_list.at(0   );
        running = now_running;
        all_threads.at(now_running)->place_ = thread::Running;
        // start_timer
        struct sigaction sa = {0};
        struct itimerval timer;

        sa.sa_handler = &timer_handler;
        if (sigaction(SIGVTALRM, &sa, NULL) < 0)
        {
            printf("sigaction error.");
        }

        timer.it_value.tv_usec = num_quantums * quantum;
        if (setitimer(ITIMER_VIRTUAL, &timer, NULL))
        {
            printf("setitimer error.");
        }
        siglongjmp(env[now_running], 1);

    }
}

void timer_handler(int) {
//    chechl if can come back if so


}
