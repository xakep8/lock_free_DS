#pragma once

#include <atomic>
#include <optional>
#include <stdexcept>
#include <thread>
#include <vector>

constexpr int MAX_THREADS = 128;

struct HazardPointer {
    std::atomic<std::thread::id> id;
    std::atomic<void*> pointer;
};

inline HazardPointer hazard_pointers[MAX_THREADS];

class HazardPointerOwner {
   public:
    HazardPointerOwner() {
        for (int i = 0; i < MAX_THREADS; i++) {
            std::thread::id old_id;
            if (hazard_pointers[i].id.compare_exchange_strong(old_id, std::this_thread::get_id(),
                                                              std::memory_order_release,
                                                              std::memory_order_relaxed)) {
                hp = &hazard_pointers[i];
                return;
            }
        }
        throw std::runtime_error("No hazard pointers available.");
    }

    std::atomic<void*>& get_pointer() { return hp->pointer; }

    ~HazardPointerOwner() {
        hp->pointer.store(nullptr);
        hp->id.store(std::thread::id());
    }

   private:
    HazardPointer* hp;
};

template <typename T>
class LockFreeQueue {
   public:
    LockFreeQueue();
    ~LockFreeQueue();
    void enqueue(const T& value);
    bool dequeue(T& result);

   private:
    struct Node {
        T data;
        std::atomic<Node*> next;
        std::atomic<bool> marked{false};

        Node() : next(nullptr) {}
        Node(const T& value) : data(value), next(nullptr) {}
    };
    void try_reclaim(std::vector<Node*>& retired_list);
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};

template <typename T>
LockFreeQueue<T>::LockFreeQueue() {
    Node* dummy = new Node();
    head.store(dummy, std::memory_order_relaxed);
    tail.store(dummy, std::memory_order_relaxed);
}

template <typename T>
LockFreeQueue<T>::~LockFreeQueue() {
    Node* node = head.exchange(nullptr, std::memory_order_acq_rel);

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
    static thread_local HazardPointerOwner hp_head_owner;
    static thread_local HazardPointerOwner hp_next_owner;
    auto& hp_head = hp_head_owner.get_pointer();
    auto& hp_next = hp_next_owner.get_pointer();

    thread_local std::vector<Node*> retired_list;

    while (true) {
        Node* head_snapshot = head.load(std::memory_order_acquire);
        hp_head.store(head_snapshot, std::memory_order_seq_cst);
        hp_next.store(nullptr, std::memory_order_seq_cst);

        if (head_snapshot != head.load(std::memory_order_acquire)) {
            continue;
        }

        Node* next_snapshot = head_snapshot->next.load(std::memory_order_acquire);
        hp_next.store(next_snapshot, std::memory_order_seq_cst);

        if (head_snapshot != head.load(std::memory_order_acquire) ||
            next_snapshot != head_snapshot->next.load(std::memory_order_acquire)) {
            continue;
        }

        if (!next_snapshot) {
            hp_head.store(nullptr, std::memory_order_seq_cst);
            hp_next.store(nullptr, std::memory_order_seq_cst);
            return false;
        }

        Node* tail_snapshot = tail.load(std::memory_order_acquire);

        if (head_snapshot == head.load(std::memory_order_acquire)) {
            if (next_snapshot == nullptr) {
                return false;
            }

            if (head_snapshot == tail_snapshot) {
                tail.compare_exchange_weak(tail_snapshot, next_snapshot, std::memory_order_release,
                                           std::memory_order_relaxed);
            } else {
                T value = next_snapshot->data;
                if (head.compare_exchange_weak(head_snapshot, next_snapshot,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                    result = value;

                    retired_list.push_back(head_snapshot);
                    if (retired_list.size() >= 32) {
                        try_reclaim(retired_list);
                    }

                    hp_head.store(nullptr, std::memory_order_seq_cst);
                    hp_next.store(nullptr, std::memory_order_seq_cst);

                    return true;
                }
            }
        }
    }
}

bool is_hazard(void* ptr) {
    for (int i = 0; i < MAX_THREADS; ++i) {
        if (hazard_pointers[i].pointer.load() == ptr) {
            return true;
        }
    }
    return false;
}

template <typename T>
void LockFreeQueue<T>::try_reclaim(std::vector<Node*>& retired_list) {
    std::vector<Node*> remaining;
    for (Node* node : retired_list) {
        if (is_hazard(node)) {
            remaining.push_back(node);
        } else {
            delete node;
        }
    }

    retired_list.swap(remaining);
}