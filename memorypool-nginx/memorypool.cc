#include "memorypool.h"

/**
 * @brief
 * @param size
 */
MemoryPool::MemoryPool(size_t size) { create(size); }

/**
 * @brief
 */
MemoryPool::MemoryPool() : pool(nullptr) {}

/**
 * @brief
 */
MemoryPool::~MemoryPool() { /*destroy();*/
}

/**
 * @brief
 * @param size
 */
void MemoryPool::create(size_t size) {
  pool = (pool_t*)Alloc(size);
  if (pool == nullptr) {
    return;
  }

  pool->data.latest = (unsigned char*)pool + sizeof(pool_t);
  pool->data.end = (unsigned char*)pool + size;
  pool->data.next = nullptr;
  pool->data.failed = 0;

  size -= sizeof(pool_t);
  pool->avaiable = (size < MAX_ALLOC_FROM_POOL) ? size : MAX_ALLOC_FROM_POOL;
  pool->current = pool;
  pool->large = nullptr;
  pool->cleanup = nullptr;
  pool->log = nullptr;
}

/**
 * @brief
 */
void MemoryPool::destroy() {
  // 执行cleanup函数
  cleanup_t* clnup;
  for (clnup = pool->cleanup; clnup; clnup = clnup->next) {
    if (clnup->func) {
      clnup->func(clnup->args);
    }
  }

  // 释放大内存
  large_t* lrg;
  for (lrg = pool->large; lrg; lrg = lrg->next) {
    if (lrg->addr) {
      free(lrg->addr);
    }
  }

  // 释放池
  pool_t *curr, *forward;
  for (curr = pool, forward = curr->data.next;;
       curr = forward, forward = forward->data.next) {
    free(curr);
    if (forward == nullptr) {
      break;
    }
  }
}

/**
 * @brief
 */
void MemoryPool::reset() {
  // 释放大内存
  for (large_t* lrg = pool->large; lrg; lrg = lrg->next) {
    if (lrg->addr) {
      free(lrg->addr);
    }
  }

  //
  for (pool_t* p = pool; p; p = p->data.next) {
    p->data.latest = (unsigned char*)p + sizeof(pool_t);
    p->data.failed = 0;
  }
  pool->current = pool;
  pool->large = nullptr;
  // TODO 是否需要重置【log & cleanup】
}

/**
 * @brief
 * @param size
 * @return
 */
void* MemoryPool::allocWithAligned(size_t size) {
  if (size <= pool->avaiable) {
    return allocSmall(size, 1);
  } else {
    return allocLarge(size);
  }
}

/**
 * @brief
 * @param size
 * @return
 */
void* MemoryPool::allocWithoutAlign(size_t size) {
  if (size <= pool->avaiable) {
    return allocSmall(size, 0);
  } else {
    return allocLarge(size);
  }
}

/**
 * @brief
 * @param size 在新分配的内存块中需要分配的内存
 * @return
 */
void* MemoryPool::allocBlock(size_t size) {
  // 与初始内存块分配相同大小的内存
  size_t psize = (size_t)(pool->data.end - (unsigned char*)pool);
  // malloc()分配原始未初始化的内存
  unsigned char* m = (unsigned char*)Alloc(psize);
  if (m == nullptr) {
    return nullptr;
  }

  pool_t* new_pool = (pool_t*)m;
  new_pool->data.end = m + psize;
  new_pool->data.failed = 0;
  new_pool->data.next = nullptr;

  // 后续添加的内存块仅包含pool_data_t的信息，pool_t的额外信息不再包含
  m += sizeof(pool_data_t);
  m = align_ptr(m, ALIGNMENT);

  new_pool->data.latest = m + size;

  // 根据内存块分配失败次数更新pool_t->current
  pool_t* curr;
  for (curr = pool->current; curr->data.next; curr = curr->data.next) {
    if (curr->data.failed++ > 4) {
      pool->current = curr->data.next;
    }
  }

  // 此时curr指向最后一个内存块，并将新创建的内存块尾部插入
  curr->data.next = new_pool;

  return m;
}

/**
 * @brief
 * @param size
 * @param aligned
 * @return 分配内存的起始地址
 */
void* MemoryPool::allocSmall(size_t size, unsigned long aligned) {
  pool_t* curr = pool->current;  // 当前内存块
  unsigned char* m;  // 当前内存块的可用空间的起始地址

  // 循环遍历所有内存块，搜索可用空间
  do {
    m = curr->data.latest;
    if (aligned) {
      m = align_ptr(m, ALIGNMENT);
    }

    // 当前内存块有足够的空间分配
    if ((size_t)(curr->data.end - m) >= size) {
      curr->data.latest = m + size;
      return m;
    }

    curr = curr->data.next;

  } while (curr);

  // 当前所有内存块中没有足够的空间 => 创建新的内存块并分配空间
  return allocBlock(size);
}

/**
 * @brief
 * @param size
 * @return 新分配的大内存块的起始地址
 */
void* MemoryPool::allocLarge(size_t size) {
  // malloc()分配原始未初始化的内存
  void* ptr = Alloc(size);
  if (ptr == nullptr) {
    return nullptr;
  }

  // 在当前大内存块节点中寻找挂载点
  unsigned long cnt = 0;
  large_t* lrg;
  for (lrg = pool->large; lrg; lrg = lrg->next) {
    // 当前大内存块为空：直接挂载新分配的大内存
    if (lrg->addr == nullptr) {
      lrg->addr = ptr;
      return ptr;
    }

    // TODO
    if (cnt++ > 3) {
      break;
    }
  }

  // 所有大内存块都不为空 =>新创建大内存块节点
  // 大内存块节点存放在内存块
  lrg = (large_t*)allocSmall(sizeof(pool_data_t), 1);
  if (lrg == nullptr) {
    free(ptr);
    return nullptr;
  }

  lrg->addr = ptr;
  lrg->next = pool->large;
  pool->large = lrg;

  return ptr;
}

/**
 * @brief 直接构建新的大内存块
 * @param size
 * @param aligned
 * @return
 */
void* MemoryPool::allocLargeWithoutSearch(size_t size, size_t aligned) {
  void* ptr = Alloc(size);
  if (ptr == nullptr) {
    return nullptr;
  }

  large_t* lrg = (large_t*)allocSmall(sizeof(large_t), aligned);
  if (!lrg) {
    free(ptr);
    return nullptr;
  }

  lrg->addr = ptr;
  lrg->next = pool->large;
  pool->large = lrg;

  return ptr;
}

/**
 * @brief 删除指定地址的大内存块
 * @param ptr
 * @return
 */
long MemoryPool::largeFree(void* ptr) {
  large_t* lrg;
  for (lrg = pool->large; lrg; lrg = lrg->next) {
    if (ptr == lrg->addr) {
      free(lrg->addr);
      lrg->addr = nullptr;

      return OK;
    }
  }

  return DECLINED;
}
