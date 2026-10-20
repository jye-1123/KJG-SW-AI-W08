#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

/**
 * 사용자 프로그램이 exit(status)로 전달한 종료 상태 보관
 * 현재는 종료 메시지 출력(process.c의 process_exit())에 사용중이고
 */
struct exit_info {  
    int status;
};

struct child_info {
    tid_t tid;                      /* 자식 번호 */
    int status;                     /* 종료 상태 */
    struct semaphore process_sema;     /* 종료 알림 */
    struct list_elem elem;          /* 부모의 children 목록 연결 */
    int refs;       /* 처음에는 부모와 자식이 각각 사용하므로 2로 시작(누가 마지막에 기록을 해제할지 판단) */
    struct lock refs_lock;      /* 부모와 자식이 refs를 동시에 변경하지 못하도록 보호 */
};



#endif /* userprog/process.h */
