.cpu cortex-m4
.syntax unified
.thumb

.section .text

.thumb_func
.global enter_user_mode
enter_user_mode:
  LDR r1, =__psp_stack_top
  MSR PSP, r1
  B _crt0
  