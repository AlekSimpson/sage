#ifndef STACK
#define STACK

#include <vector>

template<typename T>
class stack {
private:
    vector<T> vec;

public:
    // Push element onto stack
    void push(const T& x) {
        vec.push_back(x);
    }
    
    // Pop element from stack
    void pop() {
        if (!vec.empty()) {
            vec.pop_back();
        }
    }
    
    // Get top element
    T& top() {
        if (!vec.empty()) {
            return vec.back();
        }
        throw runtime_error("Stack is empty");
    }
    
    // Const version of top
    const T& top() const {
        if (!vec.empty()) {
            return vec.back();
        }
        throw runtime_error("Stack is empty");
    }
    
    // Check if stack is empty
    bool empty() const {
        return vec.empty();
    }
    
    // Get size of stack
    size_t size() const {
        return vec.size();
    }
};

#endif
