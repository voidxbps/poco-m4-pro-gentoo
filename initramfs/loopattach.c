/* Attach /data/gentoo.img to /dev/loop0 via LOOP_SET_FD + LOOP_SET_STATUS64. */
#define SYS_openat 56
#define SYS_ioctl 29
#define SYS_write 64
#define SYS_exit_group 94
#define SYS_mknodat 33
#define LOOP_SET_FD 0x4C00
#define LOOP_SET_STATUS64 0x4C04

struct loop_info64 {
    unsigned long long lo_device, lo_inode, lo_rdevice;
    unsigned long long lo_offset, lo_sizelimit;
    unsigned int lo_number, lo_encrypt_type, lo_encrypt_key_size, lo_flags;
    unsigned char lo_file_name[64], lo_crypt_name[64], lo_encrypt_key[32];
    unsigned long long lo_init[2];
};

static long sc(long n, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}

void _start(void) {
    sc(SYS_mknodat, -100, (long)"/dev/loop0", 0x61B0, (7 << 8) | 0);
    long img = sc(SYS_openat, -100, (long)"/data/gentoo.img", 2, 0);
    long dev = sc(SYS_openat, -100, (long)"/dev/loop0", 2, 0);
    long r1 = -1, r2 = -1;
    if (img >= 0 && dev >= 0) {
        r1 = sc(SYS_ioctl, dev, LOOP_SET_FD, img, 0);
        struct loop_info64 info;
        char *p = (char *)&info;
        for (unsigned long i = 0; i < sizeof(info); i++) p[i] = 0;
        const char name[] = "/data/gentoo.img";
        for (int i = 0; i < 17; i++) info.lo_file_name[i] = name[i];
        r2 = sc(SYS_ioctl, dev, LOOP_SET_STATUS64, (long)&info, 0);
    }
    /* log result to kmsg */
    long k = sc(SYS_openat, -100, (long)"/dev/kmsg", 1, 0);
    if (k >= 0) {
        if (r1 == 0 && r2 == 0) {
            const char *msg = "K2-LOOP OK\n";
            sc(SYS_write, k, (long)msg, 11, 0);
        } else {
            const char *msg = "K2-LOOP FAIL img/dev/r1/r2:\n";
            sc(SYS_write, k, (long)msg, 29, 0);
            const char *simg = (img >= 0) ? "+" : "-";
            const char *sdev = (dev >= 0) ? "+" : "-";
            const char *sr1 = (r1 >= 0) ? "+" : "-";
            const char *sr2 = (r2 >= 0) ? "+" : "-";
            const char *mimg = img >= 0 ? "IMGOK\n" : "IMGFAILED\n";
            const char *mdev = dev >= 0 ? "DEVOK\n" : "DEVFAILED\n";
            const char *mr1 = r1 >= 0 ? "SETFDOK\n" : "SETFDFAILED\n";
            const char *mr2 = r2 >= 0 ? "STATUSOK\n" : "STATUSFAILED\n";
            sc(SYS_write, k, (long)mimg, img >= 0 ? 6 : 10, 0);
            sc(SYS_write, k, (long)mdev, dev >= 0 ? 6 : 10, 0);
            sc(SYS_write, k, (long)mr1, r1 >= 0 ? 8 : 12, 0);
            sc(SYS_write, k, (long)mr2, r2 >= 0 ? 9 : 13, 0);
        }
    }
    sc(SYS_exit_group, (r1 == 0 && r2 == 0) ? 0 : 1, 0, 0, 0);
    for (;;) {}
}
