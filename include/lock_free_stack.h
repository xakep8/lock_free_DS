#pragma once

#include <atomic>

template <typename T>
class LockFreeStack {
   private:
    typedef struct Node {
        T data;
        Node* next;

        Node(const T& value) : data(value), next(nullptr) {}
    } Node;

    std::atomic<Node*> head;

   public:
    LockFreeStack() : head(nullptr) {};
    ~LockFreeStack();
    void push(const T& value);
    bool pop(T& result);
};