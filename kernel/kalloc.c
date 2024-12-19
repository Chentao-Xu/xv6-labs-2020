// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[];  // first address after kernel.
                    // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmems[NCPU];

void kinit() {
  // 注意：只有CPU0调用了kinit

  // 初始化每个CPU的锁
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmem");
  }

  freerange(end, (void *)PHYSTOP);
}

void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa) {
  push_off();
  uint id = cpuid();
  pop_off();

  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP) panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run *)pa;

  acquire(&kmems[id].lock);
  r->next = kmems[id].freelist;
  kmems[id].freelist = r;
  release(&kmems[id].lock);
}

// Pop a frame from CPUi
// Returns a pointer of run
// Returns 0 if the freelist of CPUi is empty
void *poplist(uint id) {
  struct run *r;

  acquire(&kmems[id].lock);
  r = kmems[id].freelist;
  if (r) kmems[id].freelist = r->next;
  release(&kmems[id].lock);

  return (void *)r;
}

// Steal a frame from other cpu
// Returns a pointer of run if success
// Returns 0 if no frame can be stolen
void *steal(uint currid) {
  struct run *r;

  for (int i = 0; i < NCPU; i++) {
    if (i == currid) continue;
    r = poplist(i);
    if (r) break;
  }

  return (void *)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *kalloc(void) {
  push_off();
  uint id = cpuid();
  pop_off();

  struct run *r;

  // pop from CPUi's freelist
  r = poplist(id);

  // if current CPU's freelist is empty, steal form other CPUs
  if (!r) {
    r = steal(id);
  }

  if (r) memset((char *)r, 5, PGSIZE);  // fill with junk
  return (void *)r;
}