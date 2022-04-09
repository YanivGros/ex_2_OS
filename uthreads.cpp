
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

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


class Thread {
public:
    enum place {
        Ready, Running, Blocked
    };
    bool is_blocked = false;
    int time_sleep = 0;
    int id;
    int sum_quantums = 0;
    place place;
    char stack[STACK_SIZE]{0};

    Thread(int id, enum place place) :
            id(id), place(place) {

    }
};

typedef std::priority_queue<int, std::vector<int>, std::greater<int> > min_heap;
typedef std::map<int, Thread *> map_thread;
typedef std::unique_ptr<Thread> thread_ptr;
typedef std::vector<int> vector_int;

class Scheduler {
public:
    int quantum_usecs = 0;
    int time_counter = 1;
    min_heap free_id_list;
    sigset_t *sig_mask_set;

    int free_id_list_top_pop() {
        int ret = free_id_list.top();
        free_id_list.pop();
        return ret;
    }

    map_thread in_use_threads_map;

    int id_running = 0;

    vector_int ready_list;

    int ready_list_top_pop() {
        if (ready_list.empty()) { return -1; }
        int ret = ready_list.at(0);
        ready_list.erase(ready_list.begin());
        return ret;
    }

    vector_int blocked;

    sigjmp_buf env[MAX_THREAD_NUM]{};

    void update_sleepers() {
        vector_int temp_blocked = blocked;
        for (auto t: blocked) {
            auto thr = in_use_threads_map.at(t);
            if (thr->time_sleep) {
                thr->time_sleep = thr->time_sleep - 1;
            }
            if (thr->time_sleep == 0 && !thr->is_blocked) {
                thr->place = Thread::Ready;
                ready_list.push_back(t);
                temp_blocked.erase(std::remove(temp_blocked.begin(), temp_blocked.end(), t));
            }
        }
        blocked = temp_blocked;
    }
};


bool start_time(int usecs);

void time_handler(int);

void move_to_next_ready_thread(bool move_to_ready);

void print_error(std::string msg);

bool reset_timer();

Scheduler sc;

int uthread_init(int quantum_usecs) {
    /*
     * check usec valid
     */
    if (quantum_usecs <= 0) {
        print_error("invalid quantum time, should be positive integer");
        return -1;
    }

    /*
     * Init
     */
    for (int i = 1; i < MAX_THREAD_NUM; ++i) {
        sc.free_id_list.push(i);
    }
    auto first_thread = new Thread(0, Thread::Running);
    first_thread->sum_quantums = 1;
    sc.in_use_threads_map[0] = first_thread;
    sc.quantum_usecs = quantum_usecs;
    sigemptyset(sc.sig_mask_set);
    sigaddset(sc.sig_mask_set, SIGVTALRM);
    /*
     * set SIGVTALRM
     */
    struct sigaction sa = {0};
    sa.sa_handler = &time_handler;
    sa.sa_flags = 10;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        print_error("sigaction error.");
        return -1;
    }

    /*
     * set timer to quantum_usecs
     */
    struct itimerval timer = {0};
    timer.it_value.tv_sec = 0;        // first time interval, seconds part
    timer.it_value.tv_usec = quantum_usecs;        // first time interval, microseconds part
    timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
    timer.it_interval.tv_usec = quantum_usecs;
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL)) {
        print_error("setitimer error.");
        return -1;
    }
    return 0;
}

int uthread_spawn(thread_entry_point entry_point) {
    // signal mask, wouldn't want to be interrupt in the middle of

    /*
     * check if there are more free id
     */
    sigprocmask(SIG_BLOCK, sc.sig_mask_set, NULL);

    if (sc.free_id_list.empty()) {
        print_error("there are no more free id");
        return -1;
    }

    /*
     * Init
     */
    int free_id = sc.free_id_list_top_pop();
    auto thread = new Thread(free_id, Thread::Ready);

    /*
     * put the new thread in ready_list
     */
    sc.ready_list.push_back(free_id);
    sc.in_use_threads_map.insert(std::pair<int, Thread *>(free_id, thread));
    address_t sp = (address_t) thread->stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(sc.env[free_id], 1);
    (sc.env[free_id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (sc.env[free_id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&sc.env[free_id]->__saved_mask);
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);
    return free_id;
}

int uthread_terminate(int tid) {
    sigprocmask(SIG_BLOCK, sc.sig_mask_set, NULL);
    /*
     * check if the thread is the 0 thread
     */

    if (tid == 0) {
        for (auto &item: sc.in_use_threads_map) {
            delete item.second; // releasing all resource
        }
        exit(EXIT_SUCCESS);
    }

    /*
     * check validity
     */
    if (sc.in_use_threads_map.count(tid) == 0) {
        print_error("doesnt match any in use thread id");
        return -1;
    }

    /*
     * main
     */
    auto cur_thread = sc.in_use_threads_map.at(tid);
    sc.in_use_threads_map.erase(tid);
    switch (cur_thread->place) {
        case Thread::Ready:
            sc.ready_list.erase(std::remove(sc.ready_list.begin(), sc.ready_list.end(), tid));
            break;
        case Thread::Blocked:
            sc.blocked.erase(std::remove(sc.blocked.begin(), sc.blocked.end(), tid));
            break;
    }
    sc.free_id_list.push(tid);
    if(cur_thread->place == Thread::Running){
        move_to_next_ready_thread(false);
    }
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);
    return 0;
}

void time_handler(int) {
    sigprocmask(SIG_BLOCK, sc.sig_mask_set, NULL);
    sc.time_counter++;
    sc.update_sleepers();
    move_to_next_ready_thread(true);
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);
}


void move_to_next_ready_thread(bool move_to_ready) {
    if (sc.ready_list.empty()) {
        sc.in_use_threads_map.at(sc.id_running)->sum_quantums++;
        return;
    }
    auto next = sc.ready_list_top_pop();
    sc.in_use_threads_map.at(next)->sum_quantums++;
    sc.in_use_threads_map.at(next)->place = Thread::Running;
    if (move_to_ready) {
        sc.ready_list.push_back(sc.id_running);
        sc.in_use_threads_map.at(sc.id_running)->place = Thread::Ready;
    }
    int ret_val = sigsetjmp(sc.env[sc.id_running], 1);
    bool did_just_save_bookmark = ret_val == 0;
    if (did_just_save_bookmark) {
        sc.id_running = next;
//        printf("jumping to %d \n", next);
        siglongjmp(sc.env[next], 1);
    }
}

void print_error(std::string msg) { fprintf(stderr, "thread library error: %s", msg.c_str()); }

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid) {
    sigprocmask( SIG_BLOCK, sc.sig_mask_set, NULL);
    if (tid == 0 || sc.in_use_threads_map.count(tid) == 0) {
        print_error("doesnt match any in use thread id");
        return -1;
    }
    auto cur_thread = sc.in_use_threads_map.at(tid);
    if (cur_thread->is_blocked) {
        return 0;
    }
    if (cur_thread->place == Thread::Ready) {
        sc.ready_list.erase(std::remove(sc.ready_list.begin(), sc.ready_list.end(), tid));
        sc.blocked.push_back(tid);
        cur_thread->place = Thread::Blocked;
        cur_thread->is_blocked = true;
    } else {
        sc.blocked.push_back(tid);
        cur_thread->place = Thread::Blocked;
        cur_thread->is_blocked = true;
        move_to_next_ready_thread(false);
    }
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);
    return 0;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid) {
    if (sc.in_use_threads_map.count(tid) == 0) {
        print_error("doesnt match any in use thread id");
        return -1;
    }
    auto cur_thread = sc.in_use_threads_map.at(tid);
    if (cur_thread->place == Thread::Ready || cur_thread->place == Thread::Running) {
        return 0;
    }
    cur_thread->is_blocked = false;
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);
    return 0;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY threads list.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid==0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums) {
    sigprocmask(SIG_BLOCK, sc.sig_mask_set, NULL);

    // need to mask
    if (sc.id_running == 0) {
        print_error("rani go home you're tired");
        return -1;
    }
    auto cur_thread = sc.in_use_threads_map.at(sc.id_running);
    cur_thread->place = Thread::Blocked;
    cur_thread->time_sleep = num_quantums;
    sc.blocked.push_back(sc.id_running);
    if (!reset_timer()) {
        print_error("setitimer error.");
        return -1;
    }
    move_to_next_ready_thread(false);
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);
    return 0;
}

bool reset_timer() {
    struct itimerval timer = {0};
    timer.it_value.tv_sec = 0;        // first time interval, seconds part
    timer.it_value.tv_usec = sc.quantum_usecs;        // first time interval, microseconds part
    timer.it_interval.tv_sec = 0;    // following time intervals, seconds part
    timer.it_interval.tv_usec = sc.quantum_usecs;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL)) {
        return false;
    }
    return true;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid() { return sc.id_running; }


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums() { return sc.time_counter; }


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid) {

    sigprocmask(SIG_BLOCK, sc.sig_mask_set, NULL);
    if (sc.in_use_threads_map.count(tid) == 0) {
        print_error("doesnt match any in use thread id");
        return -1;
    }
    sigprocmask(SIG_UNBLOCK, sc.sig_mask_set, NULL);

    return sc.in_use_threads_map.at(tid)->sum_quantums;
}

