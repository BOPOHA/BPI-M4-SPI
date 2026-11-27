"""Exercise the actual clock handoff against unknown state and active DMA."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class ClockHandoffTests(unittest.TestCase):
    def test_only_audited_idle_handoffs_can_change_clock(self):
        source=(ROOT/'src/u-boot/sd.c').read_text()
        registers=source[source.index('#define DMA1'):source.index('struct bpi_sd_plat')]
        function=source[source.index('static int legacy_init'):source.index('static int wait_transfer')]
        harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#define __iomem
typedef uint8_t u8;
typedef uint32_t u32;
'''+registers+r'''
static unsigned writes;
static u32 readl(void *p) { u32 v;memcpy(&v,p,4);return v; }
static void writel(u32 v,void *p) { writes++;memcpy(p,&v,4); }
static void writeb(u8 v,void *p) { writes++;*(u8 *)p=v; }
static void sync_bus(void) { }
static void udelay(unsigned n) { assert(n<=2000); }
'''+function+r'''
static void init(bool uhs) {
    memset((void *)0x98000000,0,0x50000);
    writel(uhs?0x65e3:0x2003,(void *)0x980001e0);
    writel(uhs?0x00b64388:0x00564388,(void *)0x980001e8);
    writel(7,(void *)0x980001ec);
    writel(0x24a92000,(void *)0x9804e008);
    writel(1,(void *)0x9804e00c);
    /* Unrelated GPIO and pinmux contents must survive every transition. */
    writel(0x12345678,(void *)0x98007100);
    writel(0x87654321,(void *)0x98007104);
    writel(0x400,(void *)0x9804e030);
    writes=0;
}
int main(void) {
    assert(mmap((void *)0x98000000,0x50000,PROT_READ|PROT_WRITE,
                MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0)==(void *)0x98000000);
    struct bpi_sd h={.regs=(void *)0x98010400};
    init(false);assert(!legacy_init(&h));assert(!writes);
    init(true);assert(!legacy_init(&h));assert(writes);
    assert(readl((void *)0x980001e0)==0x2003);
    assert(readl((void *)0x980001e8)==0x00564388);
    assert(readl((void *)0x98007100)==0x12345678);
    assert(readl((void *)0x98007104)==0x87654321);
    assert(readl((void *)0x9804e030)==0x400);
    assert(readl((void *)0x9804e008)==0x24a92000);
    assert(readl((void *)0x9804e00c)==1);
    /* Real second physical boot: RX phase 6 instead of phase 5. */
    init(true);writel(0x66e3,(void *)0x980001e0);
    assert(!legacy_init(&h));assert(readl((void *)0x980001e0)==0x2003);
    /* Both documented five-bit phase fields may use their boundary values. */
    init(true);writel(0x6003 | 0x1ff8,(void *)0x980001e0);
    assert(!legacy_init(&h));assert(readl((void *)0x980001e0)==0x2003);
    for(unsigned fault=0;fault<4;fault++) {
        init(true);
        if(fault==0)writel(0xdeadbeef,(void *)0x980001e0);
        if(fault==1)writel(1,h.regs+DMA3);
        if(fault==2)writel(1,(void *)0x9804e008);
        if(fault==3)writel(1,h.regs+0x74);
        writes=0;assert(legacy_init(&h));assert(!writes);
    }
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);(p/'test.c').write_text(harness)
            subprocess.run(['cc','-Wall','-Wextra','-Werror',p/'test.c','-o',p/'test'],check=True)
            subprocess.run([p/'test'],check=True,stdout=subprocess.PIPE)
