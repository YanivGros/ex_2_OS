//
// Created by qywoe on 03/04/2022.
//
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <deque>
#include <list>
#include <assert.h>
#include "uthreads.h"
//#include "libuthread.a"
#include <iostream>
#include <vector>

using namespace std;

void f (void)
{
    while(1);
}

int main(void)
{
    std::vector<int> v = {7, 5, 16, 8};
    auto b = v;
    b.push_back(20);

    int q[2] = {10, 20};
    if (uthread_init(10) == -1)
    {
        return 0;
    }
    for (int i = 0; i < 90; i++)
        cout << uthread_spawn(f) << endl;
    uthread_terminate(10);
    for (;;){
    }

}
/*
 *  uthread_terminate(5);

    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;

    uthread_terminate(15);
    uthread_terminate(25);
    uthread_terminate(35);
    uthread_terminate(45);
    uthread_terminate(55);
    uthread_terminate(65);
    uthread_terminate(75);
    uthread_terminate(85);

    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;
    cout << uthread_spawn(f) << endl;



    uthread_terminate(0);
    return 0;
 */





//#include <iostream>
//#include "uthreads.h"
//
//#include <memory>
//
//class foo {
//public:
//    foo(int a, int b) : a(a), b(b) {}
//
//public:
//    int a, b;
//};
//
//int main() {
//    uthread_init(2);
//    for (int i = 0; i < 10; ++i) {
//        uthread_spawn(nullptr);
//    }
//    for (int i = 0; i < 10; ++i) {
//        uthread_terminate(i);
//
//    }
//    for (int i = 0; i < 10; ++i) {
//        uthread_spawn(nullptr);
//    }
//    printf("hello");
//    return 0;
//}
