/**
 * @file   swi_stubs.S
 *
 * @brief  Stub functions for newlib and 349libc syscalls
 *
 * @date
 * @author
 */

.cpu cortex-m4
.syntax unified
.section .swi_stub
.thumb

#include "../../kernel/include/svc_num.h"

.global _sbrk
_sbrk:
  svc SVC_SBRK
  bx lr
  bkpt

.global _write
_write:
  svc SVC_WRITE
  bx lr
  bkpt

.global _close
_close:
  svc SVC_CLOSE
  bx lr
  bkpt

.global _fstat
_fstat:
  svc SVC_FSTAT
  bx lr
  bkpt

.global _isatty
_isatty:
  svc SVC_ISATTY
  bx lr
  bkpt

.global _lseek
_lseek:
  svc SVC_LSEEK
  bx lr
  bkpt

.global _read
_read:
  svc SVC_READ
  bx lr
  bkpt

.global _exit
_exit:
  svc SVC_EXIT 
  bx lr
  bkpt

.type thread_init, %function
.global thread_init
thread_init:
  svc SVC_THR_INIT 
  bx lr
  bkpt

.type thread_create, %function
.global thread_create
thread_create:
  svc SVC_THR_CREATE
  bx lr
  bkpt

.type thread_kill, %function
.global thread_kill
thread_kill:
  svc SVC_THR_KILL
  bx lr
  bkpt

.type scheduler_start, %function
.global scheduler_start
scheduler_start:
  svc SVC_SCHD_START
  bx lr
  bkpt

.type mutex_init, %function
.global mutex_init
mutex_init:
  svc SVC_MUT_INIT 
  bx lr
  bkpt

.type mutex_lock, %function
.global mutex_lock
mutex_lock:
  svc SVC_MUT_LOK
  bx lr
  bkpt

.type mutex_unlock, %function
.global mutex_unlock
mutex_unlock:
  svc SVC_MUT_ULK
  bx lr
  bkpt

.type wait_until_next_period, %function
.global wait_until_next_period
wait_until_next_period:
  svc SVC_WAIT
  bx lr
  bkpt

.global get_time
get_time:
  svc SVC_TIME
  bx lr
  bkpt

.global get_priority
get_priority:
  svc SVC_PRIORITY
  bx lr
  bkpt

.global thread_time
thread_time:
  svc SVC_THR_TIME
  bx lr
  bkpt

.global servo_enable
servo_enable:
  svc SVC_SERVO_ENABLE
  bx lr
  bkpt

.global servo_set
servo_set:
  svc SVC_SERVO_SET
  bx lr
  bkpt

/* The following stubs are not required to be implemented */

.global _start
_start:
  bkpt

.global _kill
_kill:
  bkpt
  
.global _gettimeofday
_gettimeofday:
  bkpt

.global _times
_times:
  bkpt

.global _getpid
_getpid:
  bkpt

.thumb_func
.global idle_default
idle_default:
  bkpt
 
 
 