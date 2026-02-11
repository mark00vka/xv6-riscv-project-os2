#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "slab.h"

extern pagetable_t kernel_pagetable;
#include "proc.h"
#include "vm.h"
#include "slab.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kkill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// Helper to get string argument from user space
static int
fetchstr_safe(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  if(copyinstr(p->pagetable, buf, addr, max) < 0)
    return -1;
  return 0;
}

uint64
sys_kmem_init(void)
{
  uint64 space;
  int block_num;

  argaddr(0, &space);
  argint(1, &block_num);

  struct proc *p = myproc();
  // uint64 va = PGROUNDDOWN(space);
  // uint64 size = (uint64)block_num * BLOCK_SIZE;
  // uint64 pa = walkaddr(p->pagetable, va);
  // if (pa == 0) return -1;
  //
  // if (!ismapped(kernel_pagetable, va))
  //   if (mappages(kernel_pagetable, va, size, pa, PTE_R | PTE_W) != 0)
  //     return -1;
  //
  // sfence_vma();

  for (uint64 va = PGROUNDDOWN(space); va < space + (uint64)block_num * BLOCK_SIZE; va += PGSIZE) {
    uint64 pa = walkaddr(p->pagetable, va);
    if (pa == 0) return -1;
    if (!ismapped(kernel_pagetable, va)) {
      if (mappages(kernel_pagetable, va, PGSIZE, pa, PTE_R | PTE_W) != 0)
        return -1;
    }
  }
  sfence_vma();

  kmem_init((void*)space, block_num);
  return 0;
}

uint64
sys_kmem_cache_create(void)
{
  uint64 name_addr;
  int size;
  uint64 ctor_addr;
  uint64 dtor_addr;

  argaddr(0, &name_addr);
  argint(1, &size);
  argaddr(2, &ctor_addr);
  argaddr(3, &dtor_addr);

  char name[64];
  if(fetchstr_safe(name_addr, name, sizeof(name)) < 0)
    return 0;

  kmem_cache_t *cache = kmem_cache_create(name, (size_t)size, (void*)ctor_addr, (void*)dtor_addr);

  return (uint64)cache;
}

uint64
sys_kmem_cache_shrink(void)
{
  uint64 cachep;

  argaddr(0, &cachep);

  if(cachep == 0)
    return -1;

  int result = kmem_cache_shrink((kmem_cache_t *)cachep);
  return (uint64)result;
}

uint64
sys_kmem_cache_alloc(void)
{
  uint64 cachep;

  argaddr(0, &cachep);

  if(cachep == 0)
    return 0;

  void *obj = kmem_cache_alloc((kmem_cache_t *)cachep);
  return (uint64)obj;
}

uint64
sys_kmem_cache_free(void)
{
  uint64 cachep;
  uint64 objp;

  argaddr(0, &cachep);
  argaddr(1, &objp);

  if(cachep == 0 || objp == 0)
    return 0;

  kmem_cache_free((kmem_cache_t *)cachep, (void *)objp);
  return 0;
}

uint64
sys_kmalloc(void)
{
  int size;

  argint(0, &size);

  if(size <= 0)
    return 0;

  void *ptr = kmalloc((size_t)size);
  return (uint64)ptr;
}

uint64
sys_kfree(void)
{
  uint64 objp;

  argaddr(0, &objp);

  if(objp == 0)
    return 0;

  kfree((void *)objp);
  return 0;
}

uint64
sys_kmem_cache_destroy(void)
{
  uint64 cachep;

  argaddr(0, &cachep);

  if(cachep == 0)
    return 0;

  kmem_cache_destroy((kmem_cache_t *)cachep);
  return 0;
}

uint64
sys_kmem_cache_info(void)
{
  uint64 cachep;

  argaddr(0, &cachep);

  if(cachep == 0)
    return 0;

  kmem_cache_info((kmem_cache_t *)cachep);
  return 0;
}

uint64
sys_kmem_cache_error(void)
{
  uint64 cachep;

  argaddr(0, &cachep);

  if(cachep == 0)
    return 1;

  int error = kmem_cache_error((kmem_cache_t *)cachep);
  return (uint64)error;
}