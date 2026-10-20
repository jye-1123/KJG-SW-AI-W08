#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <debug.h>
#include "threads/synch.h"

struct file;

extern struct lock filesys_lock;

void syscall_init (void);
/* 종료 상태를 기록하고 공통 프로세스 정리 경로로 진입함. */
void sys_exit (int status);

/* fd 테이블 */
void fd_table_create (void);
int fd_add (struct file *file);
void fd_close (int fd);
void fd_table_close_all (void);

#endif /* userprog/syscall.h */
