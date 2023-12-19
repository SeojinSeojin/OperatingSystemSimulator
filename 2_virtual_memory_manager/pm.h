#ifndef PHYSICAL_MEMORY_H
#define PHYSICAL_MEMORY_H

#include <cstring>
#include <iostream>
#include <optional>
#include <vector>

class RealMemory {
public:
  explicit RealMemory(size_t size) : size(size), memory(new char[size * 4]) {
    std::memset(memory, 0, size * 4);
    std::cout << "RealMemory Size: " << size << " addresses" << std::endl;
    for (int i = 0; i < size; i++) {
      used.push_back(false);
    }
  }

  ~RealMemory() { delete[] memory; }

  void logInfo() {
    for (size_t i = 0; i < size; ++i) {
      if (used[i]) {
        std::cout << "Address " << i << " | Data: ";
        for (size_t j = 0; j < 4; ++j) {
          char c = memory[i * 4 + j];
          if (c != '\0')
            std::cout << c;
        }
        std::cout << std::endl;
      }
    }
  }

  void writeToAddress(size_t address, const std::vector<char> &value) {
    if (address < size && value.size() <= 4) {
      std::memcpy(memory + address * 4, value.data(), value.size());
      markUsed(address, true);
    }
  }

  void flushAddress(size_t address) {
    if (address < size) {
      std::memset(memory + address * 4, 0, 4);
      markUsed(address, false);
    }
  }

  std::optional<std::vector<char>> getValue(size_t address) {
    if (address < size) {
      return std::vector<char>(memory + address * 4, memory + address * 4 + 4);
    }
    return std::nullopt;
  }

  size_t getFirstEmptyAddress() {
    for (size_t i = 0; i < size; ++i) {
      if (!used[i]) {
        return i;
      }
    }
    return -1;
  }

  bool isFull() {
    for (size_t i = 0; i < size; ++i) {
      if (!used[i]) {
        return false;
      }
    }
    return true;
  }

  void markUsed(size_t address, bool state) {
    if (address < size) {
      used[address] = state;
    }
  }

private:
  size_t size;
  char *memory;
  std::vector<bool> used;
};

#endif // PHYSICAL_MEMORY_H
