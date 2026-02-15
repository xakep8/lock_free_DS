#include "lock_free_stack.h"

#include <iostream>

template <typename T>
LockFreeStack<T>::~LockFreeStack() {}

template <typename T>
void LockFreeStack<T>::push(const T& value) {
    Node* node = (Node*)malloc(sizeof(Node));
}

template <typename T>
bool LockFreeStack<T>::pop(T& result) {}