#pragma once

#include <atomic>
#include <optional>

template <typename T>
class LockFreeQueue {
   public:
    LockFreeQueue();
    ~LockFreeQueue();
    void enqueue(const T& value);
    bool dequeue(T& result);

   private:
    struct Node {
        std::optional<T> data;
        std::atomic<Node*> next;
        std::atomic<bool> marked{false};

        Node() : data(std::nullopt), next(nullptr) {}
        Node(const T& value) : data(value), next(nullptr) {}
    };
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    std::atomic<Node*> retired;
};

template <typename T>
LockFreeQueue<T>::LockFreeQueue() {
    Node* dummy = new Node();
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
    retired.store(nullptr, std::memory_order_relaxed);
}

template <typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    Node* node = head.exchange(nullptr, std::memory_order_acq_rel);

    while (node != nullptr) {
        Node* next = node->next.load(std::memory_order_relaxed);
        delete node;
        node = next;
    }

    node = retired.exchange(nullptr, std::memory_order_acq_rel);

    while (node != nullptr) {
        Node* next = node->next.load(std::memory_order_relaxed);
        delete node;
        node = next;
    }

    tail.store(nullptr, std::memory_order_relaxed);
}

template <typename T>
void LockFreeQueue<T>::enqueue(const T& value) {
    Node* new_node = new Node(value);

    while (true) {
        Node* tail_snapshot = tail.load(std::memory_order_acquire);
        Node* next_snapshot = tail_snapshot->next.load(std::memory_order_acquire);
        if (tail_snapshot == tail.load(std::memory_order_acquire)) {
            if (next_snapshot == nullptr) {
                if (tail_snapshot->next.compare_exchange_weak(next_snapshot, new_node,
                                                              std::memory_order_release,
                                                              std::memory_order_relaxed)) {
                    tail.compare_exchange_weak(tail_snapshot, new_node, std::memory_order_release,
                                               std::memory_order_relaxed);
                    return;
                }
            } else {
                tail.compare_exchange_weak(tail_snapshot, next_snapshot, std::memory_order_release,
                                           std::memory_order_relaxed);
            }
        }
    }
}

template <typename T>
bool LockFreeQueue<T>::dequeue(T& result) {
    while (true) {
        Node* head_snapshot = head.load(std::memory_order_acquire);
        Node* tail_snapshot = tail.load(std::memory_order_acquire);
        Node* next_snapshot = head_snapshot->next.load(std::memory_order_acquire);
        if (head_snapshot == head.load(std::memory_order_acquire)) {
            if (next_snapshot == nullptr) {
                return false;
            }

            if (head_snapshot == tail_snapshot) {
                tail.compare_exchange_weak(tail_snapshot, next_snapshot, std::memory_order_release,
                                           std::memory_order_relaxed);
            } else {
                T value = *next_snapshot->data;
                if (head.compare_exchange_weak(head_snapshot, next_snapshot,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                    result = value;

                    Node* old_retired = retired.load(std::memory_order_relaxed);
                    do {
                        head_snapshot->next.store(old_retired, std::memory_order_relaxed);
                    } while (!retired.compare_exchange_weak(old_retired, head_snapshot,
                                                            std::memory_order_release,
                                                            std::memory_order_relaxed));

                    return true;
                }
            }
        }
    }
}