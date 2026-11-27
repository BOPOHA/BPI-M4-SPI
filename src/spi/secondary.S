/* SPDX-License-Identifier: GPL-2.0-only */
/* Copied to 0x30000 before releasing CPUs 1-3. The retained FSBL branches
 * there even though it loads the SPI U-Boot at 0x100000. Keep this loop
 * outside Linux's RAM bank; no stack, external calls or relocated pointers.
 * Linux's RTD1395 entry handles the EL3 -> EL2 transition.
 */
#include <config.h>
#include <linux/linkage.h>

.text
.balign 8
.global bpi_m4_secondary_pen_end
ENTRY(bpi_m4_secondary_pen)
    mrs x0, CurrentEL
    cmp x0, #0xc
    b.eq el3
    cmp x0, #0x8
    b.ne park
    b el2
el3:
    mrs x1, actlr_el3
    orr x1, x1, #2
    msr actlr_el3, x1
el2:
    mrs x1, actlr_el2
    orr x1, x1, #2
    msr actlr_el2, x1
    mrs x1, actlr_el1
    orr x1, x1, #2
    msr actlr_el1, x1
    isb
    /* Preserve the coherency/access setup used by the existing loader. */
    mrs x1, S3_1_C15_C2_1
    orr x1, x1, #0x40
    msr S3_1_C15_C2_1, x1
    isb
    movz x16, #(CPU_RELEASE_ADDR & 0xffff)
    movk x16, #((CPU_RELEASE_ADDR >> 16) & 0xffff), lsl #16
poll:
    wfe
    ldr w0, [x16]
    cbz w0, poll
    br x0
park:
    wfe
    b park
ENDPROC(bpi_m4_secondary_pen)
bpi_m4_secondary_pen_end:
