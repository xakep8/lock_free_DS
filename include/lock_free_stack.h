#pragma once

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>

constexpr uint64_t EPOCH_INACTIVE = UINT64_MAX;
inline std::atomic<uint64_t> global_epoch{0};

struct ThreadRecord {
    std::atomic<uint64_t> active_epoch{EPOCH_INACTIVE};
};

inline std::vector<ThreadRecord*> thread_registry;
inline std::mutex registry_mutex;

inline thread_local ThreadRecord* local_record = [] {
    auto* rec = new ThreadRecord();

    std::lock_guard<std::mutex> lock(registry_mutex);
    thread_registry.push_back(rec);

    return rec;
}();

struct EpochGuard {
    EpochGuard() {
        uint64_t epoch = global_epoch.load(std::memory_order_acquire);
        local_record->active_epoch.store(epoch, std::memory_order_release);
    }

    ~EpochGuard() { local_record->active_epoch.store(EPOCH_INACTIVE, std::memory_order_release); }
};

template <typename Node>
struct RetiredNode {
    Node* node;
    uint64_t retire_epoch;
};

inline thread_local uint64_t local_epoch = EPOCH_INACTIVE;

template <typename T>
class LockFreeStack {
   private:
    typedef struct Node {
        T data;
        Node* next;

        Node(const T& value) : data(value), next(nullptr) {}
    } Node;

    Node* head;
    std::mutex stack_mutex;

   public:
    static inline thread_local std::vector<RetiredNode<Node>> retire_list;

   public:
    LockFreeStack();
    ~LockFreeStack();
    void push(const T& value);
    bool pop(T& result);
    void retire_node(Node* node);
    void try_reclaim();
};

// Template implementations
template <typename T>
LockFreeStack<T>::LockFreeStack() {
    head = nullptr;
}

template <typename T>
LockFreeStack<T>::~LockFreeStack() {
    std::lock_guard<std::mutex> lock(stack_mutex);
    Node* node = head;

    while (node) {
        Node* next = node->next;
        delete node;
        node = next;
    }

    for (const auto& retired : retire_list) {
        delete retired.node;
    }
    retire_list.clear();
}

template <typename T>
void LockFreeStack<T>::push(const T& value) {
    std::lock_guard<std::mutex> lock(stack_mutex);
    Node* new_node = new Node(value);
    new_node->next = head;
    head = new_node;
}

template <typename T>
bool LockFreeStack<T>::pop(T& result) {
    std::lock_guard<std::mutex> lock(stack_mutex);
    if (!head) return false;

    Node* node = head;
    head = node->next;
    result = node->data;
    delete node;
    return true;
}

template <typename T>
void LockFreeStack<T>::try_reclaim() {
    uint64_t min_epoch = global_epoch.load(std::memory_order_acquire);

    {
        std::lock_guard<std::mutex> lock(registry_mutex);

        for (ThreadRecord* rec : thread_registry) {
            uint64_t active = rec->active_epoch.load(std::memory_order_acquire);
            if (active != EPOCH_INACTIVE) min_epoch = std::min(min_epoch, active);
        }
    }

    auto it = retire_list.begin();
    while (it != retire_list.end()) {
        if (it->retire_epoch + 2 <= min_epoch) {
            delete it->node;
            it = retire_list.erase(it);
        } else {
            ++it;
        }
    }
}

template <typename T>
void LockFreeStack<T>::retire_node(Node* node) {
    delete node;
}
