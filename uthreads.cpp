
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


class thread {
public:
    enum place {
        Ready, Running, Blocked
    };
    thread_entry_point entry_point_;
    int id_;
    place place_;
    int quantoms;
    char stack[STACK_SIZE]{0};

    thread(thread_entry_point entryPoint, int id, place place) :
            entry_point_(entryPoint), id_(id), place_(place), quantoms(1) {
    }
};

typedef std::priority_queue<int, std::vector<int>, std::greater<int> > min_heap;
typedef std::map<int, thread *> map;
typedef std::vector<int> vector_int;

void timer_handler(int);

void pose_cur();

void jump_to_thread(int list);

class scheduler {
    min_heap free_id_list;
    map all_threads;
    vector_int blocked;
    vector_int ready_list;
    int running{};
public:
    int getRunning() const {
        return running;
    }

    void setRunning(int running) {
        scheduler::running = running;
    }

private:
    int quantum{};
public:

    sigjmp_buf env[MAX_THREAD_NUM]{};

    /* quantum handle */
    void setQuantum(int quant) {
        scheduler::quantum = quant;
    }

    /* free id list handle */
    void push_free_id(int n) {
        free_id_list.push(n);
    }

    bool is_id_empty() {
        return free_id_list.empty();
    }

    int top_and_pop_free_id() {
        int ret = free_id_list.top();
        free_id_list.pop();
        return ret;
    }

    /* ready list handle */
    void add_ready_list(int i) {
        ready_list.push_back(i);
    }

    int ready_list_top_pop() {
        if (ready_list.empty()) {
            return -1;
        }
        int ret = ready_list.at(0);
        ready_list.erase(ready_list.begin());
        return ret;
    }

    /* all_threads_handle*/
    void add_to_all_threads(std::pair<int, thread *> pair1) {
        all_threads.insert(pair1);
    }

    thread *find_in_all_thread(int i) {
        return all_threads.at(i);
    }

    map *get_all_threads() {
        return &all_threads;
    }

    thread* get_thread(int i) {
        return all_threads.at(i);
    }

    int remove_thread(int i) {
        auto erase_thread = all_threads.at(i);
        all_threads.erase(i);
        switch (erase_thread->place_) {
            case thread::Running:
                jump_to_thread(ready_list_top_pop())
                timer_handler(0);
        }
    }
};


scheduler sc;

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */

address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

int uthread_init(int quantum_usecs) {

    if (quantum_usecs <= 0) {
        printf("setitimer error.");
        return -1;
    }
    sc.setQuantum(quantum_usecs);

    for (int i = 1; i < MAX_THREAD_NUM; ++i) {
        sc.push_free_id(i);
    }

    sc.setRunning(0);

    struct sigaction sa = {0};
    struct itimerval timer = {0};
    sa.sa_handler = &timer_handler;
    sa.sa_flags = 10;

    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        fprintf(stderr, "sigaction error.");
        return -1;
    }

    timer.it_value.tv_usec = quantum_usecs;        // first time interval, microseconds part
    timer.it_interval.tv_usec = quantum_usecs;    // following time intervals, microseconds part
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL)) {
        printf("setitimer error.");
        return -1;
    }
    return 0;
}

void timer_handler(int sig) {
    int ret_val = sigsetjmp(sc.env[sc.getRunning()], 1);
//    printf("yield: ret_val=%d\n", ret_val);
    bool did_just_save_bookmark = ret_val == 0;
//    bool did_jump_from_another_thread = ret_val != 0;
    if (did_just_save_bookmark) {
        jump_to_thread(sc.ready_list_top_pop());
    }
}


void jump_to_thread(int tid) {
    sc.add_ready_list(sc.getRunning());
    sc.setRunning(tid);
    printf("jumping to %d \n", tid);
    siglongjmp(sc.env[tid], 1);
}


int uthread_spawn(thread_entry_point entry_point) {
    if (sc.is_id_empty()) { return -1; }
    int free_id = sc.top_and_pop_free_id();
    auto thread_ = new thread(entry_point, free_id, thread::Ready);
    sc.add_ready_list(free_id);
    address_t sp = (address_t) thread_->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(sc.env[free_id], 1);
    (sc.env[free_id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (sc.env[free_id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&sc.env[free_id]->__saved_mask);
    sc.add_to_all_threads(std::pair<int, thread *>(free_id, thread_));
    return 0;
}

int uthread_terminate(int tid) {
    int res;
    auto res_thread = sc.find_in_all_thread(tid);
    auto thread_place = res_thread->place_;
    return 0;
}

remove_from_vector(int tid) {
    sc.remove_thread(tid);
    auto cur_thread = sc.get_thread(tid);
    auto thread_place = cur_thread->place_;

    auto all_threads = sc.get_all_threads();
    thread::place cur_place = all_threads.at(tid)->place_;
    switch (cur_place) {
        case thread::Ready:
            ready_list.erase(std::remove(ready_list.begin(), ready_list.end(), tid), ready_list.end());
            return;
        case thread::Blocked:
            blocked.erase(std::remove(ready_list.begin(), ready_list.end(), tid), ready_list.end());
            return;
        case thread::Running:
            running = -1;
            return;
        default:
            return;
    }
}

