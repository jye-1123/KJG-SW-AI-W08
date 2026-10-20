#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "userprog/process.h"
#include "threads/malloc.h"
#include "threads/init.h"
/* 명령줄 복사, 메모리 할당, 사용자 주소 검사에 사용 */
#include <string.h>
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "threads/mmu.h"

#include "filesys/file.h"		// struct file

void syscall_entry (void);
void syscall_handler (struct intr_frame *);



/* 시스템 콜(System call).
 *
 * 예전에는 시스템 콜 서비스를 인터럽트 핸들러(interrupt handler)가 처리했음
 * (예: linux의 int 0x80). 하지만 x86-64에서는 제조사가 시스템 콜을 요청하는
 * 효율적인 경로인 `syscall` 명령어를 제공함.
 *
 * syscall 명령어는 모델 특정 레지스터(Model Specific Register, MSR)의
 * 값을 읽어서 동작함. 자세한 내용은 매뉴얼을 참고할 것. */

#define MSR_STAR 0xc0000081         /* 세그먼트 셀렉터(Segment selector) msr */
#define MSR_LSTAR 0xc0000082        /* 롱 모드(Long mode) SYSCALL 대상 */
#define MSR_SYSCALL_MASK 0xc0000084 /* eflags용 마스크 */

void fd_table_create(void) {
    thread_current()->fd_table = palloc_get_page(PAL_ZERO);
}

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* 인터럽트 서비스 루틴(interrupt service routine)은 syscall_entry가
	 * 유저랜드 스택을 커널 모드 스택으로 바꿀 때까지 어떤 인터럽트도
	 * 처리하면 안 됨. 따라서 FLAG_FL을 마스크함. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

static struct file *find_file_by_fd(int fd) {
	/*전달받은 fd에 해당하는 항목 찾기
 	찾았다면 해당 항목의 struct file * 반환
 	없다면 NULL 반환*/
	return NULL;
}

void sys_exit(int status) {
	struct thread *cur = thread_current();

	if (cur->child_info != NULL) {
		cur->child_info->status = status;
	}
	thread_exit();
}

static void sys_wait(struct intr_frame *f) {
	tid_t child_tid = (tid_t)f->R.rdi;

	f->R.rax = process_wait(child_tid);
}

static void sys_halt(void) {
	power_off();
}

static void sys_exec(struct intr_frame *f) {
    /* exec에 전달한 명령줄의 사용자 주소를 가져옴 */
    const char *command_line = (const char *)f->R.rdi;
    size_t length;

    /* 초안에서는 명령줄을 널 문자까지 포함해 한 페이지로 제한.
     * 문자열을 읽기 전에 각 주소가 유효한지 먼저 검사. */
    for (length = 0; length < PGSIZE; length++) {
        const char *addr = (const char *)((uintptr_t)command_line + length);

        /* 커널 주소 검사를 먼저 해서 페이지 조회의 ASSERT를 피한다. */
        if (addr == NULL || !is_user_vaddr(addr) || pml4_get_page(thread_current()->pml4, addr) == NULL) {
            sys_exit(-1);
		}

        /* 문자열 끝을 찾으면 길이 확인을 마친다. */
        if (*addr == '\0') {
            break;
		}
    }

    /* 한 페이지 안에 문자열 끝이 없으면 실행하지 않는다. */
    if (length == PGSIZE) {
        sys_exit(-1);
	}

    /* 기존 사용자 메모리가 지워져도 유지될 커널 공간을 확보한다. */
    char *copy = palloc_get_page(0);
    if (copy == NULL) {
        sys_exit(-1);
	}

    /* 끝의 널 문자까지 복사한다. */
    memcpy(copy, command_line, length + 1);

    /* copy는 기존 process_exec()가 성공·실패 모두에서 해제한다.
     * 성공하면 돌아오지 않고, 로드 실패로 돌아오면 종료한다. */
    if (process_exec(copy) < 0)
        sys_exit(-1);
}

//putbuf (const char *buffer, size_t n)
/* 시스템 콜의 메인 인터페이스 */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: 여기에 구현을 작성할 것.
	int fd = f->R.rdi;

	switch (f->R.rax) {
		case SYS_EXIT: {		/* exit(status) 첫 인자는 rdi로 전달 */
			sys_exit((int)f->R.rdi);	/* 사용자가 exit(status)에 넘긴 첫 번쨰 인자는 rdi에 들어 있음*/
			break;
		}

		case SYS_WAIT: {
			sys_wait(f);
			break;
		}

		case SYS_HALT: {
			sys_halt();
			break;
		}

		case SYS_EXEC: {
			/* 명령줄 검사와 실행 교체를 담당 함수에 맡긴다. */
			sys_exec(f);
			break;
		}

		case SYS_FILESIZE: {
			struct file *file = find_file_by_fd(fd);		// 실제 파일 조회는 이후 open 구현 후 fd 관리 코드와 연결
			
			if (file == NULL) {		// fd에 연결된 파일 없으면 실패(-1) 반환
				printf("[filesize] 파일 번호 %d에 해당하는 열린 파일 찾지 못했습니다.\n", fd);
				f->R.rax = -1;
				break;
			}
			
			int size = file_length(file);		// 파일 크기 조회해서 리턴
			printf("[filesize] 파일번호(fd) = %d, 파일 크기(size) = %d\n", fd, size);

			f->R.rax = size;

			printf("[filesize] fd = %d\n", fd);
			break;
		}

		case SYS_TELL: {
			struct file *file = find_file_by_fd(fd);

			if (file == NULL) {
				printf("[tell] 파일 번호 %d에 해당하는 열린 파일 찾지 못했습니다.\n", fd);
				f->R.rax = -1;
				break;
			}
			
			int file_loc = file_tell(file);
			printf("[tell] 파일번호(fd) = %d, 현재 위치(file_loc) = %d\n", fd, file_loc);

			f->R.rax = file_loc;

			break;
		}

		case SYS_SEEK: {
			break;
		}

		default: 
			break;
			
	}
}
