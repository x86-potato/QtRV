        .data
# 5-row x 3-column pixel font, one word per row (bits 2..0 = left..right column)
glyph_H: .word 5,5,7,5,5
glyph_E: .word 7,4,6,4,7
glyph_L: .word 4,4,4,4,7
glyph_O: .word 7,5,5,5,7

        .text
        .globl main

# Bitmap Display device assumptions (Devices > Bitmap Display > Configure... in
# the GUI) -- these are the tool's defaults, so no configuration is needed:
#   base address = 0x10040000
#   width        = 64 pixels
#   height       = 32 pixels
# Framebuffer format: one 32-bit word per pixel, 0x00RRGGBB, row-major:
#   address = base + (row * width + col) * 4
#
# The device itself is a dumb framebuffer -- everything below (the font,
# the drawing loop) runs entirely in MIPS code, exactly as it would on real
# memory-mapped display hardware.

main:
        # --- clear the framebuffer to black ---
        lui  $s0, 0x1004        # $s0 = framebuffer base (0x10040000)
        li   $t0, 2048          # width(64) * height(32) pixels
        add  $t1, $zero, $zero  # pixel index
clear_loop:
        beq  $t1, $t0, clear_done
        sll  $t2, $t1, 2
        add  $t2, $s0, $t2
        sw   $zero, 0($t2)
        addi $t1, $t1, 1
        b    clear_loop
clear_done:

        # --- draw "HELLO" ---
        # draw_glyph(a0 = glyph table addr, a1 = x offset px, a2 = y offset px)
        la   $a0, glyph_H
        li   $a1, 2
        li   $a2, 10
        jal  draw_glyph

        la   $a0, glyph_E
        li   $a1, 6
        li   $a2, 10
        jal  draw_glyph

        la   $a0, glyph_L
        li   $a1, 10
        li   $a2, 10
        jal  draw_glyph

        la   $a0, glyph_L
        li   $a1, 14
        li   $a2, 10
        jal  draw_glyph

        la   $a0, glyph_O
        li   $a1, 18
        li   $a2, 10
        jal  draw_glyph

        addi $v0, $zero, 10
        syscall

# void draw_glyph(a0 = glyph table addr (5 words, one per row),
#                 a1 = x offset px, a2 = y offset px)
draw_glyph:
        addi $sp, $sp, -4
        sw   $ra, 0($sp)

        add  $t0, $zero, $zero      # row = 0
draw_row:
        addi $t9, $zero, 5
        beq  $t0, $t9, draw_done
        sll  $t1, $t0, 2
        add  $t1, $a0, $t1
        lw   $t2, 0($t1)            # this row's 3-bit column mask

        add  $t3, $zero, $zero      # col = 0
draw_col:
        addi $t9, $zero, 3
        beq  $t3, $t9, row_next

        # bit = (mask >> (2 - col)) & 1
        addi $t4, $zero, 2
        sub  $t4, $t4, $t3
        srlv $t5, $t2, $t4
        andi $t5, $t5, 1
        beq  $t5, $zero, col_next

        # pixel address = FB_BASE + ((y+row)*WIDTH + (x+col)) * 4
        add  $t6, $a2, $t0          # y + row
        li   $t7, 64                # WIDTH
        mult $t6, $t7
        mflo $t6
        add  $t8, $a1, $t3          # x + col
        add  $t6, $t6, $t8
        sll  $t6, $t6, 2
        lui  $t7, 0x1004
        add  $t6, $t6, $t7
        li   $t7, 0xFFFFFF          # white
        sw   $t7, 0($t6)

col_next:
        addi $t3, $t3, 1
        b    draw_col

row_next:
        addi $t0, $t0, 1
        b    draw_row

draw_done:
        lw   $ra, 0($sp)
        addi $sp, $sp, 4
        jr   $ra
