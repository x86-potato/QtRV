        .text
        .globl main

# Fills the entire Bitmap Display framebuffer with a solid color -- a
# minimal version of the fill loop used by initializeDisplay in the real
# project's Display.asm.
#
# Defaults match the tool's Bitmap Display defaults exactly (Devices >
# Bitmap Display): base 0x10040000, 64x32 pixels. If you resize the display
# in Configure..., update FB_WORDS below to match (width * height) -- e.g.
# 128x128 -> 16384.

main:
        lui  $t0, 0x1004        # $t0 = framebuffer base (0x10040000)
        li   $t1, 2048          # FB_WORDS = width * height (64*32 = 2048)
        li   $t2, 0x00FF3366    # fill color, 0x00RRGGBB

fill_loop:
        beqz $t1, done
        sw   $t2, 0($t0)
        addi $t0, $t0, 4
        subi $t1, $t1, 1
        b    fill_loop

done:
        addi $v0, $zero, 10
        syscall
