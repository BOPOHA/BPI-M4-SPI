/* SPDX-License-Identifier: GPL-2.0-only */
/* RTD1395 ACTLR access setup adapted from the pinned Realtek start.S.
 * Copyright (C) 2015 Realtek. DDR remains owned by the retained FSBL.
 * Upstream start.S supplies vectors, timer frequency and SMPEN setup.
 */
#include <linux/linkage.h>
#include <asm/macro.h>
#include <config.h>
ENTRY(lowlevel_init)
    mov x29, lr
    switch_el x1, 3f, 2f, 1f
3:  mrs x0, actlr_el3
    orr x0, x0, #2
    msr actlr_el3, x0
2:  mrs x0, actlr_el2
    orr x0, x0, #2
    msr actlr_el2, x0
1:  mrs x0, actlr_el1
    orr x0, x0, #2
    msr actlr_el1, x0
    isb
    /* Match the vendor EL3 GICv2 setup. FSBL may enter at EL2 instead. */
    switch_el x1, 3f, 2f, 2f
3:  branch_if_slave x0, 1f
    ldr x0, =GICD_BASE
    bl gic_init_secure
1:  ldr x0, =GICD_BASE
    ldr x1, =GICC_BASE
    bl gic_init_secure_percpu
2:  mov lr, x29
    ret
ENDPROC(lowlevel_init)
