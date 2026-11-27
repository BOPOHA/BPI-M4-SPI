"""Run actual U-Boot command/completion C with controlled MMIO faults."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT = Path(__file__).resolve().parents[1]

class UbootSDTests(unittest.TestCase):
    def test_read_payload_is_withheld_on_completion_faults(self):
        source=(ROOT/'src/u-boot/sd.c').read_text()
        registers=source[source.index('#define DMA1'):source.index('struct bpi_sd_plat')]
        functions=source[source.index('static int wait_transfer'):source.index('static int set_ios')]
        harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#define __iomem
#define MMC_RSP_PRESENT 1
#define MMC_RSP_136 2
#define MMC_RSP_CRC 4
#define MMC_RSP_BUSY 8
#define MMC_DATA_READ 1
#define MMC_DATA_WRITE 2
typedef uint8_t u8;
typedef uint32_t u32;
typedef unsigned long ulong;
'''+registers+r'''
struct udevice { struct bpi_sd *host; };
struct mmc_cmd { unsigned int cmdidx, resp_type, cmdarg; u32 response[4]; };
struct mmc_data { unsigned int blocksize, blocks, flags; u8 *dest; };
static u8 registers[1024], dma_buffer[512];
static unsigned int ticks, fault;
static struct bpi_sd *dev_get_priv(struct udevice *d) { return d->host; }
static u8 readb(void *p) { return *(u8 *)p; }
static void writeb(u8 v, void *p) { *(u8 *)p=v; }
static u32 readl(void *p) { u32 v; memcpy(&v,p,4);return v; }
static void writel(u32 v,void *p) { memcpy(p,&v,4); }
static u32 get_unaligned_be32(const u8 *p) { return (u32)p[0]<<24|(u32)p[1]<<16|(u32)p[2]<<8|p[3]; }
static void sync_bus(void) { }
static void udelay(unsigned int delay) { (void)delay; }
static ulong get_timer(ulong base) {
    if (!ticks++) {
        memset(dma_buffer,0x5a,sizeof(dma_buffer));
        registers[TRANS]=fault==1?0x10:0x60;
        if (fault!=2) writel(0,registers+DMA3);
        if (fault==3) registers[ST1]=0x82;
        if (fault==4) registers[ST2]=1;
        if (fault==5) registers[CMD0+1]=0x80; /* R1 OUT_OF_RANGE */
        if (fault==6) registers[CMD0+1]=0x02; /* R1 CARD_IS_LOCKED */
    }
    return (ticks-1)*1100-base;
}
'''+functions+r'''
int main(void) {
    struct bpi_sd host={.regs=registers,.buffer=dma_buffer,.data_clock=0x2101};
    struct udevice dev={&host};
    struct mmc_cmd cmd={.cmdidx=17,.resp_type=MMC_RSP_PRESENT|MMC_RSP_CRC,.cmdarg=2048};
    u8 output[512];
    struct mmc_data data={.blocksize=512,.blocks=1,.flags=MMC_DATA_READ,.dest=output};
    for (fault=0;fault<=6;fault++) {
        ticks=0;host.failed=false;memset(registers,0,sizeof(registers));memset(output,0xa5,512);
        int ret=send_cmd(&dev,&cmd,&data);
        if (!fault) {
            assert(!ret && !host.failed);
            for (int i=0;i<512;i++) assert(output[i]==0x5a);
            assert(readl(registers+CKGEN)==0x2101);
        } else {
            assert(ret && host.failed);
            for (int i=0;i<512;i++) assert(output[i]==0xa5);
            memset(dma_buffer,0x77,512);
            assert(send_cmd(&dev,&cmd,&data)==-EIO);
            for (int i=0;i<512;i++) assert(dma_buffer[i]==0x77);
        }
    }
    host.failed=false;cmd.cmdidx=18;assert(send_cmd(&dev,&cmd,&data)==-EOPNOTSUPP);
    cmd.cmdidx=17;data.flags=MMC_DATA_WRITE;assert(send_cmd(&dev,&cmd,&data)==-EINVAL);
    data.flags=MMC_DATA_READ;data.blocks=2;assert(send_cmd(&dev,&cmd,&data)==-EINVAL);
    data.blocks=1;data.blocksize=1024;assert(send_cmd(&dev,&cmd,&data)==-EINVAL);
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder);(p/'test.c').write_text(harness)
            subprocess.run(['cc','-Wall','-Wextra','-Werror',p/'test.c','-o',p/'test'],check=True)
            subprocess.run([p/'test'],check=True,stdout=subprocess.PIPE)
