.data
sieve:      .word 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
str_prompt: .asciiz "Enter N (2-100): "
str_pre:    .asciiz "Primes up to "
str_colon:  .asciiz ":\n"
str_sp:     .asciiz " "
str_nl:     .asciiz "\n"

.text
.globl main

# Sieve of Eratosthenes -- N supplied at runtime
#   s0 = sieve base address
#   s1 = N  (user input, clamped to [2,100])
#   s2 = floor(sqrt(N))
#   t0 = loop index i
#   t2 = inner loop index j
#   t3 = sieve[i] value
#   t5 = byte address of sieve[i/j]
#   t9 = comparison scratch

main:
    la   $s0, sieve

    # Print prompt and read N
    la   $a0, str_prompt
    addi $v0, $zero, 4
    syscall
    addi $v0, $zero, 5
    syscall
    add  $s1, $v0, $zero

    # Clamp N to [2, 100]
    slti $t9, $s1, 2
    bne  $t9, $zero, clamp_lo
    addi $t9, $zero, 100
    slt  $t1, $t9, $s1
    beq  $t1, $zero, clamp_done
    addi $s1, $zero, 100
    beq  $zero, $zero, clamp_done
clamp_lo:
    addi $s1, $zero, 2
clamp_done:

    # Compute floor(sqrt(N)): find largest s2 where (s2+1)^2 > N
    addi $s2, $zero, 1
sqrt_loop:
    addi $t0, $s2, 1
    mult $t0, $t0
    mflo $t1
    slt  $t9, $s1, $t1
    bne  $t9, $zero, sqrt_done
    addi $s2, $s2, 1
    beq  $zero, $zero, sqrt_loop
sqrt_done:

    # Initialize sieve[0..N] = 1
    addi $t0, $zero, 0
init:
    slt  $t9, $s1, $t0
    bne  $t9, $zero, init_done
    sll  $t5, $t0, 2
    add  $t5, $s0, $t5
    addi $t1, $zero, 1
    sw   $t1, 0($t5)
    addi $t0, $t0, 1
    beq  $zero, $zero, init
init_done:
    sw   $zero, 0($s0)
    sw   $zero, 4($s0)

    # Outer sieve loop: i = 2 .. sqrt(N)
    addi $t0, $zero, 2
outer:
    slt  $t9, $s2, $t0
    bne  $t9, $zero, print_phase
    sll  $t5, $t0, 2
    add  $t5, $s0, $t5
    lw   $t3, 0($t5)
    beq  $t3, $zero, outer_next
    mult $t0, $t0
    mflo $t2
inner:
    slt  $t9, $s1, $t2
    bne  $t9, $zero, outer_next
    sll  $t5, $t2, 2
    add  $t5, $s0, $t5
    sw   $zero, 0($t5)
    add  $t2, $t2, $t0
    beq  $zero, $zero, inner
outer_next:
    addi $t0, $t0, 1
    beq  $zero, $zero, outer

print_phase:
    la   $a0, str_pre
    addi $v0, $zero, 4
    syscall
    add  $a0, $s1, $zero
    addi $v0, $zero, 1
    syscall
    la   $a0, str_colon
    addi $v0, $zero, 4
    syscall

    addi $t0, $zero, 2
print_loop:
    slt  $t9, $s1, $t0
    bne  $t9, $zero, done
    sll  $t5, $t0, 2
    add  $t5, $s0, $t5
    lw   $t3, 0($t5)
    beq  $t3, $zero, print_next
    add  $a0, $t0, $zero
    addi $v0, $zero, 1
    syscall
    la   $a0, str_sp
    addi $v0, $zero, 4
    syscall
print_next:
    addi $t0, $t0, 1
    beq  $zero, $zero, print_loop

done:
    la   $a0, str_nl
    addi $v0, $zero, 4
    syscall
    addi $v0, $zero, 10
    syscall
