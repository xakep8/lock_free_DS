#pragma once

#include <atomic>
#include <vector>

inline std::atomic<uint64_t> global_epoch{0};
inline thread_local uint64_t local_epoch = 0;

template <typename T>
class LockFreeStack {
   private:
    typedef struct Node {
        T data;
        Node* next;
        std::atomic<bool> marked{false};

        Node(const T& value) : data(value), next(nullptr) {}
    } Node;

    std::atomic<Node*> head;
    std::atomic<Node*> retired_head;

   public:
    LockFreeStack();
    ~LockFreeStack();
    void push(const T& value);
    bool pop(T& result);
    void retire_node(Node* node, uint64_t retire_epoch);
    void try_advance_epoch();
    void graceful_cleanup();
};

// Template implementations
template <typename T>
LockFreeStack<T>::LockFreeStack() {
    head.store(nullptr, std::memory_order_relaxed);
    retired_head.store(nullptr, std::memory_order_relaxed);
    local_epoch = global_epoch.load(std::memory_order_acquire);
}

template <typename T>
LockFreeStack<T>::~LockFreeStack() {
    graceful_cleanup();
}

template <typename T>
void LockFreeStack<T>::push(const T& value) {
    uint64_t epoch = global_epoch.load(std::memory_order_acquire);
    local_epoch = epoch;

    Node* new_node = new Node(value);

    Node* old_head = head.load(std::memory_order_relaxed);
    do {
        new_node->next = old_head;
    } while (!head.compare_exchange_weak(old_head, new_node, std::memory_order_release,
                                         std::memory_order_relaxed));
}

template <typename T>
bool LockFreeStack<T>::pop(T& result) {
    uint64_t epoch = global_epoch.load(std::memory_order_acquire);
    local_epoch = epoch;

    while (true) {
        Node* old_head = head.load(std::memory_order_acquire);

        if (!old_head) {
            return false;
        }

        if (old_head->marked.load(std::memory_order_acquire)) {
            continue;
        }

        Node* new_head = old_head->next;

        if (head.compare_exchange_weak(old_head, new_head, std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
            result = old_head->data;
            retire_node(old_head, epoch);
            return true;
        }
    }
}

struct RetiredNode {
    void* node;
    uint64_t retire_epoch;
};

inline thread_local std::vector<RetiredNode> local_retired_list;

template <typename T>
void LockFreeStack<T>::retire_node(Node* node, uint64_t retire_epoch) {
    node->marked.store(true, std::memory_order_release);

    Node* old_retired = retired_head.load(std::memory_order_relaxed);
    do {
        node->next = old_retired;
    } while (!retired_head.compare_exchange_weak(old_retired, node, std::memory_order_release,
                                                 std::memory_order_relaxed));
}

template <typename T>
void LockFreeStack<T>::try_advance_epoch() {
    // No-op: all cleanup deferred to destructor
}

template <typename T>
void LockFreeStack<T>::graceful_cleanup() {
    Node* node = head.exchange(nullptr, std::memory_order_acq_rel);

    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }

    node = retired_head.exchange(nullptr, std::memory_order_acq_rel);

    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }

    for (const auto& rn : local_retired_list) {
        delete reinterpret_cast<Node*>(rn.node);
    }
    local_retired_list.clear();
}
