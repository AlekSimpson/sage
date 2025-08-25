#pragma once
#include <vector>
#include <functional>

template<typename T>
struct ascending_list {
    T* data = nullptr;
    int capacity = 0;
    int size = 0;

    ascending_list() {}

    ascending_list(int capacity) : capacity(capacity) {
        data = new T[capacity];
    }

    // Copy constructor
    ascending_list(const ascending_list& other) : capacity(other.capacity), size(other.size) {
        if (other.data != nullptr && capacity > 0) {
            data = new T[capacity];
            for (int i = 0; i < size; ++i) {
                data[i] = other.data[i];
            }
        } else {
            data = nullptr;
        }
    }

    // Copy assignment operator
    ascending_list& operator=(const ascending_list& other) {
        if (this != &other) {
            // Clean up existing data
            if (data != nullptr) {
                delete[] data;
                data = nullptr;
            }
            
            // Copy from other
            capacity = other.capacity;
            size = other.size;
            if (other.data != nullptr && capacity > 0) {
                data = new T[capacity];
                for (int i = 0; i < size; ++i) {
                    data[i] = other.data[i];
                }
            } else {
                data = nullptr;
            }
        }
        return *this;
    }

    // Move constructor
    ascending_list(ascending_list&& other) noexcept : data(other.data), capacity(other.capacity), size(other.size) {
        other.data = nullptr;
        other.capacity = 0;
        other.size = 0;
    }

    // Move assignment operator
    ascending_list& operator=(ascending_list&& other) noexcept {
        if (this != &other) {
            // Clean up existing data
            if (data != nullptr) {
                delete[] data;
            }
            
            // Move from other
            data = other.data;
            capacity = other.capacity;
            size = other.size;
            
            // Reset other
            other.data = nullptr;
            other.capacity = 0;
            other.size = 0;
        }
        return *this;
    }

    // ~ascending_list() {
    //     if (data != nullptr) {
    //         delete[] data;
    //     }
    // }

    void insert_before(int index, T item) {
        T temp;
        T place = item;
        for (int cursor = index - 1; cursor <= size; ++cursor) {
            temp = data[cursor];
            data[cursor] = place;
            place = temp;
        }
        size++;
    }

    void insert(T item, std::function<int(T)> sorter) {
        if (size == capacity || capacity == 0) {
            return;
        }

        if (size == 0) {
            data[0] = item;
            size++;
            return;
        }

        if (sorter(data[0]) > sorter(item)) {
            insert_before(0, item);
            return;
        }

        auto biggest_element = sorter(data[size - 1]);
        auto item_element = sorter(item);
        if (biggest_element < item_element || biggest_element == item_element) {
            data[size] = item;
            size++;
            return;
        }

        int search_space = size;
        int cursor = search_space / 2;
        while (true) {
            search_space = search_space / 2;
            if (sorter(data[cursor]) == sorter(item)) {
                insert_before(cursor, item);
                return;
            }

            if (sorter(data[cursor]) < sorter(item)) {
                if (cursor + 1 < size && sorter(data[cursor + 1]) > sorter(item)) {
                    // item is greater than cursor but less than an element directly after cursor
                    insert_before(cursor + 1, item);
                    return;
                }

                cursor = cursor + search_space; // right half
                continue;
            }

            cursor = cursor - search_space; // left half
        }
    }

    T &operator[](int index) {
        return data[index];
    }

    vector<T> to_vector() {
        return vector<T>(data, data + size);
    }

};
