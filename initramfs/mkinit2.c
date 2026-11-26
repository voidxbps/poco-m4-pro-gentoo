/* PID1 init: mount /data, log, then exec shell script. Correct arm64 syscalls. */
#define SYS_mknodat 33
#define SYS_mount 40
#define SYS_openat 56
#define SYS_write 64
#define SYS_execve 221
#define SYS_exit_group 94

static long sc(long n, long a, long b, long c, long d) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
static long sc6(long n, long a, long b, long c, long d, long e) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a;
    register long x1 __asm__("x1") = b;
    register long x2 __asm__("x2") = c;
    register long x3 __asm__("x3") = d;
    register long x4 __asm__("x4") = e;
    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4) : "memory");
    return x0;
}

void _start(void) {
    sc(SYS_mknodat, -100, (long)"/dev/block/userdata", 0x61B0, (259 << 8) | 43);
    long m = sc6(SYS_mount, (long)"/dev/block/userdata", (long)"/data", (long)"f2fs", 0, 0);
    sc(SYS_mknodat, -100, (long)"/dev/kmsg", 0x21A0, (1 << 8) | 11);
    long kfd = sc(SYS_openat, -100, (long)"/dev/kmsg", 1, 0);
    if (kfd >= 0) {
        const char *res = (m == 0) ? "K2-ELF mount OK\n" : "K2-ELF mount FAIL\n";
        sc(SYS_write, kfd, (long)res, (m == 0) ? 16 : 18, 0);
    }
    long fd = sc(SYS_openat, -100, (long)"/data/kernel-init-ran", 0x241, 0644);
    if (fd >= 0) {
        const char s1[] = "K2-ELF mount=";
        sc(SYS_write, fd, (long)s1, sizeof(s1) - 1, 0);
        sc(SYS_write, fd, (long)&m, sizeof(m), 0);
        const char s2[] = "\n";
        sc(SYS_write, fd, (long)s2, 1, 0);
    }
    const char *envp[] = { "PATH=/bin:/sbin:/usr/bin:/usr/sbin", 0 };
    const char *argv[] = { "/system/bin/sh", "/gentoo-init.sh", 0 };
    sc(SYS_execve, (long)"/system/bin/sh", (long)argv, (long)envp, 0);
    sc(SYS_exit_group, 1, 0, 0, 0);
    for (;;) {}
}
