.section .text
.global getSth
.type getSth, @function

getSth:
  xorq %rsi, %rsi

cond:
  cmpq $10000000, %rsi
  # jmp if %rsi < 1000
  jge fin

loop:
  addq $1, (%rdi)
  incq %rsi
  jmp cond

fin:
  ret
