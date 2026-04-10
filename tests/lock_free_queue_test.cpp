#include "lock_free_queue.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

TEST(LockFreeQueueTests, EnqueueDequeueSingleThread) {
    LockFreeQueue<int> queue;

    queue.enqueue(10);
    queue.enqueue(20);

    int value;
    EXPECT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 10);

    EXPECT_TRUE(queue.dequeue(value));
    EXPECT_EQ(value, 20);

    EXPECT_FALSE(queue.dequeue(value));
}