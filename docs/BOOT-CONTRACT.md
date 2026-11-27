Original Banana Pi M4 (RTD1395, 2 GiB). SPI loads SD root extlinux versioned
kernel/DTB/generic initramfs bundles. Server uses full RAM; Media requires
`bananapi,spi-media-handoff = "v1"` and compatible reserved memory.
FSBL CPU0 entry is 0x100000; secondary entry is 0x30000 with the verified
112-byte waiting loop. Kernel mailbox and EL3-to-EL2 patches must match.
Kernel updates do not require SPI rebuilds. Physical power cycling is the
supported mode switch. Media and eMMC boot remain unqualified.
