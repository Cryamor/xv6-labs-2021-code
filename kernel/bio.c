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

#define NBUFMAP 13
#define NBUFMAP_HASH(dev, blockno) ((((dev)<<27)|(blockno))%NBUFMAP)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buf bufmap[NBUFMAP];
  struct spinlock bufmap_lock[NBUFMAP];
} bcache;

void
binit(void)
{
  char lockname[17];
  for(int i = 0; i < NBUFMAP; i++){
    snprintf(lockname, sizeof(lockname), "bcache_bufmap_%d", i);
    initlock(&bcache.bufmap_lock[i], lockname);
    bcache.bufmap[i].next = 0;
  }

  for(int i = 0; i < NBUF; i++){
    struct buf *b = &bcache.buf[i];
    snprintf(lockname, sizeof(lockname), "buffer_%d", i);
    initsleeplock(&b->lock, lockname);
    b->lru = 0;
    b->refcnt = 0;
    b->next = bcache.bufmap[0].next;
    bcache.bufmap[0].next = b;
  }

  initlock(&bcache.lock, "bcache");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  uint key = NBUFMAP_HASH(dev, blockno);
  acquire(&bcache.bufmap_lock[key]);

  // Is the block already cached?
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.bufmap_lock[key]);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached.
  // 为了避免死锁，先释放自己的小锁，然后上大锁
  release(&bcache.bufmap_lock[key]);
  acquire(&bcache.lock);
  // 再次检查是否被修改了
  for(b = bcache.bufmap[key].next; b; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      acquire(&bcache.bufmap_lock[key]);
      b->refcnt++;
      release(&bcache.bufmap_lock[key]);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  } 


  struct buf *last = 0;
  uint lru_key = -1;

  for(int i = 0; i < NBUFMAP; i++){
    acquire(&bcache.bufmap_lock[i]);
    int found = 0;
    for(b = &bcache.bufmap[i]; b->next; b = b->next){
      if(b->next->refcnt == 0 &&
        (!last || b->next->lru < last->next->lru)){
          last = b;
          found = 1;
      }
    }
    if(!found) // 没有找到更合适的选择（LRU），保持锁
      release(&bcache.bufmap_lock[i]);
    else{ // 找到更合适的，释放之前的锁并更新记录
      if(lru_key != -1) release(&bcache.bufmap_lock[lru_key]);
      lru_key = i;
    }
  }

  if(!last) panic("bget: no buffers");

  b = last->next;
  if(lru_key != key){
    last->next = b->next;
    release(&bcache.bufmap_lock[lru_key]);
    acquire(&bcache.bufmap_lock[key]);
    b->next = bcache.bufmap[key].next;
    bcache.bufmap[key].next = b;
  }
  b->dev = dev;
  b->blockno = blockno;
  b->refcnt = 1; // 第二次检查时有refcnt++
  b->valid = 0;
  release(&bcache.bufmap_lock[key]);
  release(&bcache.lock);
  acquiresleep(&b->lock);

  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint key = NBUFMAP_HASH(b->dev, b->blockno);

  acquire(&bcache.bufmap_lock[key]);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->lru = ticks;
  }
  
  release(&bcache.bufmap_lock[key]);
}

void
bpin(struct buf *b) {
  uint key = NBUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_lock[key]);
  b->refcnt++;
  release(&bcache.bufmap_lock[key]);
}

void
bunpin(struct buf *b) {
  uint key = NBUFMAP_HASH(b->dev, b->blockno);
  acquire(&bcache.bufmap_lock[key]);
  b->refcnt--;
  release(&bcache.bufmap_lock[key]);
}


