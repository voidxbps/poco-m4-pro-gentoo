/* Draw a text console on the real DRM scanout buffer, raw ioctls, no libdrm. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <libdrm/drm_fourcc.h>
#include <time.h>
#include <pty.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <linux/fb.h>
#define DRM_MODE_CONNECTED 1
static int drmIoctl(int fd, unsigned long req, void *arg) { return ioctl(fd, req, arg); }
#include "font8x16.h"

enum { COLS=132, MAXL=120 };
static unsigned char *g_fb; static int g_w, g_h;
static char g_lines[MAXL][COLS+1]; static int g_nlines=0;
static char g_cur[COLS+1]; static int g_cpos=0, g_esc=0, g_pendcr=0;
static int g_mfd=-1, g_sfd=-1;

static void logmsg(const char *s) {
    int f = open("/data/drmterm-status", 0x441, 0644);
    if (f >= 0) { lseek(f, 0, 2); write(f, s, strlen(s)); close(f); }
}
static void redraw(void) {
    memset(g_fb,0,g_w*g_h*4);
    for(int L=0;L<g_nlines;L++) for(int ci=0;g_lines[L][ci];ci++) {
        const unsigned char *g=font8x16[(unsigned char)g_lines[L][ci]];
        for(int row=0;row<16;row++) for(int bit=0;bit<8;bit++)
            if(g[row]&(0x80>>bit)) {
                int px=8+ci*8+bit, py=8+L*18+row;
                if(px>=0&&px<g_w&&py>=0&&py<g_h) ((unsigned*)g_fb)[py*g_w+px]=0xffffffff;
            }
    }
    for(int ci=0;ci<g_cpos;ci++) {
        const unsigned char *g=font8x16[(unsigned char)g_cur[ci]];
        for(int row=0;row<16;row++) for(int bit=0;bit<8;bit++)
            if(g[row]&(0x80>>bit)) {
                int px=8+ci*8+bit, py=8+g_nlines*18+row;
                if(px>=0&&px<g_w&&py>=0&&py<g_h) ((unsigned*)g_fb)[py*g_w+px]=0xffffffff;
            }
    }
}
static void clean(char *dst,const char *src,int max) {
    int q=0, esc=0;
    for(const char *p=src;*p&&q<max;p++) {
        unsigned char c=(unsigned char)*p;
        if(esc) { if(c>=0x40&&c<=0x7e) esc=0; continue; }
        if(c==0x1b) { esc=1; continue; }
        if(c=='\t') { while(q<max&&q%8) dst[q++]=' '; continue; }
        if(c>=32&&c<127) dst[q++]=c;
    }
    dst[q]=0;
}
static void pushline(const char *src) {
    char t[COLS*2+1]; clean(t,src,COLS*2);
    int len=strlen(t), off=0;
    while(len>0) {
        int take=len>COLS?COLS:len;
        if(take==COLS) { int k=take; while(k>0&&t[k-1]!=' ') k--; if(k>take/2) take=k; }
        if(g_nlines<MAXL) { memcpy(g_lines[g_nlines],t+off,take); g_lines[g_nlines][take]=0; g_nlines++; }
        else { for(int q=0;q<MAXL-1;q++) memcpy(g_lines[q],g_lines[q+1],COLS+1); \
               memcpy(g_lines[MAXL-1],t+off,take); g_lines[MAXL-1][take]=0; }
        off+=take; len-=take;
    }
    redraw();
}
static void putcfb(int ch) {
    if(g_esc) { if(ch>=0x40&&ch<=0x7e) g_esc=0; return; }
    if(ch==0x1b) { g_esc=1; return; }
    if(ch=='\r') { g_pendcr=1; return; }
    if(ch=='\n') { g_cur[strlen(g_cur)]=0; pushline(g_cur); g_cur[0]=0; g_cpos=0; g_pendcr=0; return; }
    if(g_pendcr) { g_cur[0]=0; g_cpos=0; g_pendcr=0; }
    if(ch=='\b'||ch==0x7f) { if(g_cpos>0) g_cur[--g_cpos]=0; return; }
    if(ch=='\t') { while(g_cpos<COLS&&g_cpos%8) g_cur[g_cpos++]=' '; g_cur[g_cpos]=0; return; }
    if(ch>=32&&ch<127) { g_cur[g_cpos++]=ch; g_cur[g_cpos]=0; \
        if(g_cpos>=COLS) { pushline(g_cur); g_cur[0]=0; g_cpos=0; } }
}
static void feed(const char *b,int n) { for(int i=0;i<n;i++) putcfb((unsigned char)b[i]); redraw(); }

static pid_t spawn_login(void) {
    pid_t p=fork();
    if(p==0) {
        setsid(); ioctl(g_sfd,TIOCSCTTY,0);
        dup2(g_sfd,0); dup2(g_sfd,1); dup2(g_sfd,2);
        close(g_sfd);
        execl("/bin/login","login",NULL); _exit(1);
    }
    return p;
}
int main(void) {
    char buf[200];
    logmsg("START\n");
    int fd = open("/dev/dri/card0", O_RDWR);
    if (fd < 0) { logmsg("CARD-FAIL\n"); return 1; }
    logmsg("CARD-OK\n");
    if (drmIoctl(fd, DRM_IOCTL_SET_MASTER, 0)<0) { snprintf(buf,sizeof(buf),"MASTER-FAIL errno=%d\n",errno); logmsg(buf); } else logmsg("MASTER-OK\n");

    struct drm_mode_card_res res;
    memset(&res, 0, sizeof(res));
    if (drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) { logmsg("RES1-FAIL\n"); return 1; }
    snprintf(buf,sizeof(buf),"COUNTS c=%d e=%d crtc=%d fb=%d\n",res.count_connectors,res.count_encoders,res.count_crtcs,res.count_fbs); logmsg(buf);
    if (!res.count_connectors || !res.count_crtcs) return 1;
    uint32_t conn_ids[32]; uint32_t enc_ids[32]; uint32_t crtc_ids[32];
    if (res.count_connectors > 32) res.count_connectors = 32;
    res.connector_id_ptr = (uint64_t)(uintptr_t)conn_ids;
    res.encoder_id_ptr = (uint64_t)(uintptr_t)enc_ids;
    res.crtc_id_ptr = (uint64_t)(uintptr_t)crtc_ids;
    if (drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) { logmsg("RES2-FAIL\n"); return 1; }

    int conn_id=-1, crtc_id=-1;
    struct drm_mode_modeinfo mode; memset(&mode,0,sizeof(mode));
    for (int i=0;i<res.count_connectors;i++) {
        struct drm_mode_get_connector c; memset(&c,0,sizeof(c));
        c.connector_id = conn_ids[i];
        if (drmIoctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c)<0) continue;
        snprintf(buf,sizeof(buf),"CONN %d status=%d modes=%d enc=%d\n",c.connector_id,c.connection,c.count_modes,c.encoder_id); logmsg(buf);
        if (c.connection == DRM_MODE_CONNECTED && c.count_modes>0) {
            struct drm_mode_modeinfo modes[8]; memset(modes,0,sizeof(modes));
            uint32_t encs[16]; memset(encs,0,sizeof(encs));
            c.modes_ptr = (uint64_t)(uintptr_t)modes;
            c.encoders_ptr=(uint64_t)(uintptr_t)encs; c.count_encoders=16;
            c.count_props=0; c.props_ptr=0; c.prop_values_ptr=0;
            int mr = drmIoctl(fd, DRM_IOCTL_MODE_GETCONNECTOR, &c);
            snprintf(buf,sizeof(buf),"MODEIO=%d modes=%d enc0=%u\n",mr,c.count_modes,encs[0]); logmsg(buf);
            if (c.count_modes>0) mode = modes[0];
            conn_id = c.connector_id;
            struct drm_mode_get_encoder e; memset(&e,0,sizeof(e));
            e.encoder_id = encs[0] ? encs[0] : c.encoder_id;
            if (drmIoctl(fd, DRM_IOCTL_MODE_GETENCODER, &e)>=0) { crtc_id = e.crtc_id; logmsg("ENC-OK\n"); }
            if (crtc_id<=0 && crtc_ids[0]>0) crtc_id=crtc_ids[0];
            break;
        }
    }
    /* Build a valid mode from fb0 var info (sysfs reports 1080x2400). */
    {
        int fbd = open("/dev/fb0", O_RDWR);
        struct fb_var_screeninfo v; memset(&v,0,sizeof(v));
        if (fbd >= 0 && ioctl(fbd, FBIOGET_VSCREENINFO, &v) == 0) {
            memset(&mode,0,sizeof(mode));
            mode.hdisplay=v.xres; mode.vdisplay=v.yres;
            mode.hsync_start=v.xres+v.right_margin;
            mode.hsync_end=v.xres+v.right_margin+v.hsync_len;
            mode.htotal=v.xres+v.right_margin+v.hsync_len+v.left_margin;
            mode.vsync_start=v.yres+v.lower_margin;
            mode.vsync_end=v.yres+v.lower_margin+v.vsync_len;
            mode.vtotal=v.yres+v.lower_margin+v.vsync_len+v.upper_margin;
            mode.hdisplay=1080; mode.hsync_start=1100; mode.hsync_end=1110; mode.htotal=1120;
            mode.vdisplay=2400; mode.vsync_start=2410; mode.vsync_end=2415; mode.vtotal=2420;
            mode.vrefresh=60; mode.clock=322629;
            int n=snprintf(mode.name,32,"%dx%d",v.xres,v.yres); (void)n;
            mode.flags=0;
            snprintf(buf,sizeof(buf),"MODE from fb %dx%d pclk=%u\n",mode.hdisplay,mode.vdisplay,mode.clock); logmsg(buf);
        }
        if (fbd>=0) close(fbd);
    }
    /* override with self-consistent timings using the real pixel clock */
    memset(&mode,0,sizeof(mode));
    mode.hdisplay=1080; mode.hsync_start=1100; mode.hsync_end=1110; mode.htotal=1120;
    mode.vdisplay=2400; mode.vsync_start=2410; mode.vsync_end=2415; mode.vtotal=2420;
    mode.clock=322629; mode.vrefresh=60; mode.type=DRM_MODE_TYPE_DRIVER|DRM_MODE_TYPE_PREFERRED;
    snprintf(mode.name,32,"1080x2400");
    snprintf(buf,sizeof(buf),"SELECT conn=%d crtc=%d %dx%d clk=%u h=%d,%d,%d,%d v=%d,%d,%d,%d flags=%x type=%x\n",conn_id,crtc_id,mode.hdisplay,mode.vdisplay,mode.clock,mode.hsync_start,mode.hsync_end,mode.htotal,mode.hdisplay,mode.vsync_start,mode.vsync_end,mode.vtotal,mode.vdisplay,mode.flags,mode.type); logmsg(buf);
    if (conn_id<0||crtc_id<0) return 1;

    struct drm_mode_create_dumb dumb; memset(&dumb,0,sizeof(dumb));
    dumb.width=mode.hdisplay; dumb.height=mode.vdisplay; dumb.bpp=32;
    if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dumb)<0) { logmsg("DUMB-FAIL\n"); return 1; }
    snprintf(buf,sizeof(buf),"DUMB %ux%u pitch=%u size=%llu\n",dumb.width,dumb.height,dumb.pitch,(unsigned long long)dumb.size); logmsg(buf);

    struct drm_mode_fb_cmd2 fbcmd; memset(&fbcmd,0,sizeof(fbcmd));
    fbcmd.width=dumb.width; fbcmd.height=dumb.height;
    fbcmd.pixel_format=DRM_FORMAT_XRGB8888;
    fbcmd.handles[0]=dumb.handle; fbcmd.pitches[0]=dumb.pitch;
    if (drmIoctl(fd, DRM_IOCTL_MODE_ADDFB2, &fbcmd)<0) { logmsg("ADDFB-FAIL\n"); return 1; }
    snprintf(buf,sizeof(buf),"FBID=%u\n",fbcmd.fb_id); logmsg(buf);

    struct drm_mode_map_dumb map; memset(&map,0,sizeof(map)); map.handle=dumb.handle;
    if (drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map)<0) { logmsg("MAP-FAIL\n"); return 1; }
    unsigned char *fb = mmap(0,dumb.size,PROT_READ|PROT_WRITE,MAP_SHARED,fd,map.offset);
    if (fb==MAP_FAILED) { logmsg("MMAP-FAIL\n"); return 1; }
    logmsg("MMAP-OK\n");
    g_fb=fb; g_w=dumb.width; g_h=dumb.height;

    struct drm_mode_crtc crtc; memset(&crtc,0,sizeof(crtc));
    crtc.crtc_id=crtc_id; crtc.fb_id=fbcmd.fb_id; crtc.mode_valid=1;
    crtc.set_connectors_ptr=(uint64_t)(uintptr_t)&conn_ids[0]; /* conn_id is first? fix below */
    uint32_t sel[1]={(uint32_t)conn_id};
    crtc.set_connectors_ptr=(uint64_t)(uintptr_t)sel;
    crtc.count_connectors=1; crtc.mode=mode;
    if (drmIoctl(fd, DRM_IOCTL_MODE_SETCRTC, &crtc)<0) { snprintf(buf,sizeof(buf),"SETCRTC-FAIL errno=%d %s\n",errno,strerror(errno)); logmsg(buf); return 1; }
    logmsg("SETCRTC-OK\n");

    /* bind our framebuffer to the primary plane via legacy SETPLANE */
    struct drm_mode_get_plane_res pres; memset(&pres,0,sizeof(pres));
    drmIoctl(fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &pres);
    uint32_t pids[16]; memset(pids,0,sizeof(pids));
    pres.plane_id_ptr=(uint64_t)(uintptr_t)pids;
    drmIoctl(fd, DRM_IOCTL_MODE_GETPLANERESOURCES, &pres);
    snprintf(buf,sizeof(buf),"PLANES=%d\n", pres.count_planes); logmsg(buf);
    for (int pi=0; pi<pres.count_planes && pi<16; pi++) {
      struct drm_mode_set_plane sp; memset(&sp,0,sizeof(sp));
      sp.plane_id=pids[pi]; sp.crtc_id=crtc_id; sp.fb_id=fbcmd.fb_id;
      sp.crtc_x=0; sp.crtc_y=0; sp.crtc_w=mode.hdisplay; sp.crtc_h=mode.vdisplay;
      sp.src_x=0; sp.src_y=0; sp.src_w=((uint64_t)mode.hdisplay)<<16; sp.src_h=((uint64_t)mode.vdisplay)<<16;
      int pr=drmIoctl(fd, DRM_IOCTL_MODE_SETPLANE, &sp);
      snprintf(buf,sizeof(buf),"SETPLANE %u rc=%d errno=%d\n",pids[pi],pr,errno); logmsg(buf);
      if (pr>=0) break;
    }


    /* ---- live kernel log, then real Gentoo login on the framebuffer ---- */
    pushline("Gentoo Linux 4.14.186 (/dev/ttyFB)");
    int kfd=open("/dev/kmsg",O_RDONLY|O_NONBLOCK);
    snprintf(buf,sizeof(buf),"KMSG-FD=%d\n",kfd); logmsg(buf);
    char kb[8192];
    /* stream the real kernel log, then stop (like a normal boot) */
    for(int t=0;t<60;t++) {
        if(kfd>=0) { ssize_t n=read(kfd,kb,sizeof(kb)-1);
          if(n>0) { kb[n]=0; char *p=kb;
            while(*p) { char *e=strchr(p,'\n'); if(e)*e=0;
              char *msg=strchr(p,';'); msg=msg?msg+1:p; while(*msg==' ')msg++;
              if(*msg) pushline(msg); if(!e)break; p=e+1; } } }
        usleep(100000);
    }
    logmsg("LOG-DONE\n");
    /* real Gentoo login on a pty */
    if(openpty(&g_mfd,&g_sfd,NULL,NULL,NULL)<0){ logmsg("PTY-FAIL\n"); return 1; }
    { struct termios t; if(tcgetattr(g_sfd,&t)==0) {
        t.c_oflag |= ONLCR; t.c_oflag &= ~OCRNL; t.c_iflag |= ICRNL;
        tcsetattr(g_sfd,TCSANOW,&t); } }
    fcntl(g_mfd,F_SETFL,fcntl(g_mfd,F_GETFL)|O_NONBLOCK);
    int mfd=g_mfd, sfd=g_sfd;
    int login_pid=spawn_login();
    snprintf(buf,sizeof(buf),"LOGIN-PID=%d\n",login_pid); logmsg(buf);
    char mb[8192];
    for(int iter=0;;iter++) {
        if(iter==10||iter==400||access("/var/tmp/tty-dump",F_OK)==0) { \
            if(access("/var/tmp/tty-dump",F_OK)==0) unlink("/var/tmp/tty-dump"); \
            snprintf(buf,sizeof(buf),"/var/tmp/fb-%d.raw",iter); \
            int of=open(buf,0x241,0644); if(of>=0){write(of,g_fb,g_w*g_h*4);close(of);} }
        int st;
        if(waitpid(login_pid,&st,WNOHANG)==login_pid) {
            snprintf(buf,sizeof(buf),"LOGIN-EXIT status=%d - restarting\n",st); logmsg(buf);
            login_pid=spawn_login();
        }
        ssize_t n=read(g_mfd,mb,sizeof(mb)-1);
        if(n>0) { feed(mb,n); \
            int of=open("/var/tmp/tty-output",0x441,0644); \
            if(of>=0){ lseek(of,0,2); write(of,mb,n); close(of); } }
        int inf=open("/var/tmp/tty-input",O_RDONLY);
        if(inf>=0) { char ib[4096]; ssize_t m=read(inf,ib,sizeof(ib)); \
            if(m>0) write(g_mfd,ib,m);
            close(inf); unlink("/var/tmp/tty-input"); }
        usleep(100000);
    }
    return 0;
}
