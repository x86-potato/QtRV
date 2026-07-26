        .data
msg_li_small:  .asciiz "li (small):  "
msg_li_large:  .asciiz "li (large):  "
msg_move:      .asciiz "move:        "
msg_not:       .asciiz "not(0):      "
msg_neg:       .asciiz "neg:         "
msg_b_bad:     .asciiz "ERROR: b did not skip!\n"
msg_b_ok:      .asciiz "b (branch):  skipped correctly\n"
newline:       .asciiz "\n"

        .text
        .globl main

# Exercises every pseudo-instruction the assembler expands:
#   la (existing), li, move, nop, not, neg, b
main:
        # --- li: small immediate ---
        la   $a0, msg_li_small
        addi $v0, $zero, 4
        syscall
        li   $t0, 42
        add  $a0, $t0, $zero
        addi $v0, $zero, 1
        syscall
        la   $a0, newline
        addi $v0, $zero, 4
        syscall

        # --- li: large immediate (needs the lui+ori expansion) ---
        la   $a0, msg_li_large
        addi $v0, $zero, 4
        syscall
        li   $t1, 100000
        add  $a0, $t1, $zero
        addi $v0, $zero, 1
        syscall
        la   $a0, newline
        addi $v0, $zero, 4
        syscall

        # --- move ---
        la   $a0, msg_move
        addi $v0, $zero, 4
        syscall
        move $t2, $t0
        add  $a0, $t2, $zero
        addi $v0, $zero, 1
        syscall
        la   $a0, newline
        addi $v0, $zero, 4
        syscall

        # --- not ---
        la   $a0, msg_not
        addi $v0, $zero, 4
        syscall
        not  $t3, $zero
        add  $a0, $t3, $zero
        addi $v0, $zero, 1
        syscall
        la   $a0, newline
        addi $v0, $zero, 4
        syscall

        # --- neg ---
        la   $a0, msg_neg
        addi $v0, $zero, 4
        syscall
        neg  $t4, $t0
        add  $a0, $t4, $zero
        addi $v0, $zero, 1
        syscall
        la   $a0, newline
        addi $v0, $zero, 4
        syscall

        # --- nop + b (unconditional branch) ---
        nop
        nop
        b    after_bad
        la   $a0, msg_b_bad     # must be skipped
        addi $v0, $zero, 4
        syscall

after_bad:
        la   $a0, msg_b_ok
        addi $v0, $zero, 4
        syscall

        addi $v0, $zero, 10
        syscall
