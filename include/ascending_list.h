#pragma once
#include <vector>

template<typename T>
struct ascending_list {
  T* data = nullptr;
  int capacity = 0;
  int size = 0;
  
  sorted_list();
  sorted_list(int capacity) : capacity(capacity), size(0) {}

  void insert_before(int index, T item) {
    T temp;
    T place = item;
    for (int cursor = index-1; cursor <= size; ++cursor) {
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

    if (sorter(data[size-1]) < sorter(item)) {
      data[size] = item;
      size++;
      return
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
	if (cursor+1 < size && sorter(data[cursor+1]) > sorter(item)) {
      	  // item is greater than cursor but less than element directly after cursor
      	  insert_before(cursor+1, item);
      	  return;
      	}

	cursor = cursor + search_space; // right half
	continue;
      }

      cursor = cursor - search_space; // left half
    }
  }

  T& operator[](int index) {
    return data[index];
  }

  vector<T> to_vector() {
    return vector<T>(data, data+size);
  }
};
