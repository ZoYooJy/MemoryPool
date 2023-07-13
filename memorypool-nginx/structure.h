#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>

#define DEFAULT_POOL_SIZE (16 * 1024)
#define POOL_ALIGNMENT 16
#define PAGE_SIZE 4096
#define MAX_ALLOC_FROM_POOL (PAGE_SIZE - 1)
#define ALIGNMENT sizeof(unsigned long)

#define align(d, a) (((d) + (a - 1)) & ~(a - 1))
#define align_ptr(p, a)                                     \
  (u_char*)(((unsigned long)(p) + ((unsigned long)a - 1)) & \
            ~((unsigned long)a - 1))

#define OK 0
#define ERROR -1
#define AGAIN -2
#define BUSY -3
#define DONE -4
#define DECLINED -5
#define ABORT -6

struct cleanup_t {
  cleanup_t* next;                  //
  std::function<void(void*)> func;  //
  void* args;                       //
};

struct log_t {};

struct large_t {
  large_t* next;  //
  void* addr;     //
};

struct pool_t;
struct pool_data_t {
  unsigned char* latest;  //
  unsigned char* end;     //
  pool_t* next;           //
  unsigned long failed;   //
};

struct pool_t {
  pool_data_t data;    //
  size_t avaiable;     //
  pool_t* current;     //
  large_t* large;      //
  cleanup_t* cleanup;  //
  log_t* log;          //
};

#define NGX_MIN_POOL_SIZE \
  ngx_align((sizeof(pool_t) + 2 * sizeof(large_t)), POOL_ALIGNMENT)

/**
 * @brief malloc()的包装，不初始化
 * @param size
 * @return
 */
void* Alloc(size_t size) {
  void* ptr;
  ptr = malloc(size);
  if (ptr == nullptr) {
    std::cout << "malloc failed" << std::endl;
  }

  // std::cout << "malloc addr: " << ptr << ", malloc size: " << size <<
  // std::endl;

  return ptr;
}

/**
 * @brief calloc()的包装，实际调用Alloc()并完成0初始化
 * @param size
 * @return
 */
void* Calloc(size_t size) {
  void* ptr;
  ptr = Alloc(size);
  if (ptr) {
    memset(ptr, 0, size);
  }

  return ptr;
}

// #define memalign(alignment, size) Alloc(size)