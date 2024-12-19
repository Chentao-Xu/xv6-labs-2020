// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13

struct {
  struct spinlock locks[NBUCKETS];
  struct spinlock steallock;
  struct buf buf[NBUF];

  // a hash table that has a lock per hash bucket
  struct buf hashbuckets[NBUCKETS];
} bcache;

void binit(void) {
  struct buf *b;

  for (int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.locks[i], "bcache");

    // 初始化每个 bucket 的链表头
    bcache.hashbuckets[i].prev = &bcache.hashbuckets[i];
    bcache.hashbuckets[i].next = &bcache.hashbuckets[i];
  }

  for (int i = 0; i < NBUF; i++) {
    b = &bcache.buf[i];
    int bucket_idx = i % NBUCKETS;  // 均匀分配到各个 bucket
    initsleeplock(&b->lock, "buffer");

    // 插入到对应 bucket 的链表头
    b->next = bcache.hashbuckets[bucket_idx].next;
    b->prev = &bcache.hashbuckets[bucket_idx];
    bcache.hashbuckets[bucket_idx].next->prev = b;
    bcache.hashbuckets[bucket_idx].next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *bget(uint dev, uint blockno) {
  struct buf *b;
  uint key = blockno % NBUCKETS;

  acquire(&bcache.locks[key]);

  // Is the block already cached?
  for (b = bcache.hashbuckets[key].next; b != &bcache.hashbuckets[key]; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Is current hashbasket has unused buffer?
  for (b = bcache.hashbuckets[key].prev; b != &bcache.hashbuckets[key]; b = b->prev) {
    if (b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      release(&bcache.locks[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Only one thread can steal at a time
  acquire(&bcache.steallock);

  for (int offset = 1; offset < NBUCKETS; offset++) {
    int i = (key + offset) % NBUCKETS;

    acquire(&bcache.locks[i]);
    for (b = bcache.hashbuckets[i].prev; b != &bcache.hashbuckets[i]; b = b->prev) {
      if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;

        // delete b from bucket[i]
        b->prev->next = b->next;
        b->next->prev = b->prev;

        release(&bcache.locks[i]);

        // add b to bucket[key]
        b->next = bcache.hashbuckets[key].next;
        b->prev = &bcache.hashbuckets[key];
        bcache.hashbuckets[key].next->prev = b;
        bcache.hashbuckets[key].next = b;

        // release lock in order
        release(&bcache.steallock);
        release(&bcache.locks[key]);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.locks[i]);
  }

  // Current busket's buffers are all used.
  // Recycle the least recently used (LRU) unused buffer from a bucket.
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *bread(uint dev, uint blockno) {
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock)) panic("brelse");

  releasesleep(&b->lock);

  uint key = b->blockno % NBUCKETS;

  acquire(&bcache.locks[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.hashbuckets[key].next;
    b->prev = &bcache.hashbuckets[key];
    bcache.hashbuckets[key].next->prev = b;
    bcache.hashbuckets[key].next = b;
  }

  release(&bcache.locks[key]);
}

void bpin(struct buf *b) {
  uint key = b->blockno % NBUCKETS;
  acquire(&bcache.locks[key]);
  b->refcnt++;
  release(&bcache.locks[key]);
}

void bunpin(struct buf *b) {
  uint key = b->blockno % NBUCKETS;
  acquire(&bcache.locks[key]);
  b->refcnt--;
  release(&bcache.locks[key]);
}
