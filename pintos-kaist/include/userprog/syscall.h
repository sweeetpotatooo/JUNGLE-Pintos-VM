#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/synch.h"

/* Global filesystem lock to protect file operations across processes.
   Defined in syscall.c and shared via this extern declaration. */
extern struct lock filesys_lock;

/* Acquire filesys_lock only if the current thread does not already
   hold it.  Returns true when the lock was acquired and should be
   released later with filesys_lock_release_cond(). */
static inline bool
filesys_lock_acquire_cond (void)
{
    if (!lock_held_by_current_thread (&filesys_lock))
    {
        lock_acquire (&filesys_lock);
        return true;
    }
    return false;
}

/* Release filesys_lock only when ACQUIRED is true. */
static inline void
filesys_lock_release_cond (bool acquired)
{
    if (acquired)
        lock_release (&filesys_lock);
}

void syscall_init (void);

void seek(int fd, unsigned position);
void exit(int status) ;
void halt(void);
int write(int fd, const void *buffer, unsigned size);
int exec(char *file_name) ;
int open(const char *filename) ;
void close(int fd);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
#endif /* userprog/syscall.h */
