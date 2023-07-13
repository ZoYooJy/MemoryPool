#include <chrono>
#include <iostream>

#include "memorypool.h"

#define ALLOC_CNT 1000000
#define ALLOC_SIZE 100

void test_malloc() {
  void* memory[ALLOC_CNT];

  std::cout << "malloc" << std::endl;
  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < ALLOC_CNT; ++i) {
    memory[i] = malloc(ALLOC_SIZE);
  }
  auto end = std::chrono::steady_clock::now();
  std::cout << "malloc costs: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - beg)
                   .count()
            << " ms" << std::endl;

  std::cout << "**********************" << std::endl;

  std::cout << "free" << std::endl;
  beg = std::chrono::steady_clock::now();
  for (int i = 0; i < ALLOC_CNT; ++i) {
    free(memory[i]);
  }
  end = std::chrono::steady_clock::now();
  std::cout << "free costs: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - beg)
                   .count()
            << " ms" << std::endl;
}

void test_mp() {
  void* memory[ALLOC_CNT];
  MemoryPool pool(4096);

  std::cout << "mp malloc" << std::endl;
  auto beg = std::chrono::steady_clock::now();
  for (int i = 0; i < ALLOC_CNT; ++i) {
    memory[i] = pool.allocWithAligned(ALLOC_SIZE);
  }
  auto end = std::chrono::steady_clock::now();
  std::cout << "mp malloc costs: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - beg)
                   .count()
            << " ms" << std::endl;

  std::cout << "**********************" << std::endl;

  std::cout << "mp free" << std::endl;
  beg = std::chrono::steady_clock::now();
  pool.destroy();
  end = std::chrono::steady_clock::now();
  std::cout << "mp free costs: "
            << std::chrono::duration_cast<std::chrono::microseconds>(end - beg)
                   .count()
            << " ms" << std::endl;
}

int main() {
  test_malloc();
  std::cout << std::endl;
  test_mp();

  return 0;
}