#include "lock_free_stack.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(LockFreeStackTests, PushPopSingleThread) {
    LockFreeStack<int> stack;

    stack.push(10);
    stack.push(20);

    int value;
    EXPECT_TRUE(stack.pop(value));
    EXPECT_EQ(value, 20);

    EXPECT_TRUE(stack.pop(value));
    EXPECT_EQ(value, 10);

    EXPECT_FALSE(stack.pop(value));
}

TEST(LockFreeStackTests, ConcurrentPushPop) {
    LockFreeStack<int> stack;

    constexpr int THREADS = 4;
    constexpr int OPS_PER_THREAD = 100000;

    std::atomic<int> push_count{0};
    std::atomic<int> pop_count{0};

    auto worker = [&]() {
        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            stack.push(i);
            push_count++;

            int value;
            if (stack.pop(value)) {
                pop_count++;
            }
        }
    };

    std::vector<std::thread> threads;

    for (int i = 0; i < THREADS; ++i) threads.emplace_back(worker);

    for (auto& t : threads) t.join();

    // Stack should be empty or near empty depending on interleaving
    int remaining = 0;
    int value;

    while (stack.pop(value)) remaining++;

    EXPECT_EQ(push_count.load(), pop_count.load() + remaining);
}
