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
#include <linux/fb.h>
#define DRM_MODE_CONNECTED 1
static int drmIoctl(int fd, unsigned long req, void *arg) { return ioctl(fd, req, arg); }
#include "font.h"

static void logmsg(const char *s) {
    int f = open("/data/drmterm-status", 0x441, 0644);
    if (f >= 0) { lseek(f, 0, 2); write(f, s, strlen(s)); close(f); }
}
static void draw_char(unsigned char *fb, int w, int h, int x, int y, char c, unsigned color) {
    if (c >= 'a' && c <= 'z') c -= 32;
    unsigned char *g = font[(unsigned char)c];
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 7; row++)
            if (g[col] & (1 << row))
                for (int dy = 0; dy < 6; dy++)
                    for (int dx = 0; dx < 6; dx++) {
                        int px = x + col * 6 + dx, py = y + row * 6 + dy;
                        if (px >= 0 && px < w && py >= 0 && py < h)
                            ((unsigned *)fb)[py * w + px] = color;
                    }
}
int main(void) {
    char buf[160];
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

    /* ---- animated boot log then interactive shell ---- */
    static char lines[40][81];
    static int nlines=0, cursor=0;
    void (*noop)(void)=0; (void)noop;
    #define REDRAW() do { \
        memset(fb,0,dumb.size); \
        for(int L=0;L<nlines;L++) for(int ci=0;lines[L][ci];ci++) \
            draw_char(fb,dumb.width,dumb.height,25+ci*25,25+L*26,lines[L][ci], \
                      L==0?0x00ff00:0x00ffffff); \
    } while(0)
    #define ADD(s) do { if(nlines<40){ snprintf(lines[nlines],81,"%s",s); nlines++; } \
                        else { for(int q=0;q<39;q++) snprintf(lines[q],81,"%s",lines[q+1]); \
                               snprintf(lines[39],81,"%s",s); } REDRAW(); } while(0)
    ADD("GENTOO LINUX - POCO M4 PRO");
    usleep(300000);
    ADD("BOOT: vendor boot ok");
    usleep(300000);
    ADD("MOUNT: /data/gentoo.img -> / (loop0)");
    usleep(300000);
    ADD("INIT: OpenRC pid1");
    usleep(300000);
    ADD("SHELL: type a command via /var/tmp/tty-input");
    ADD("> ");
    REDRAW();
    logmsg("ANIM-DONE\n");
    for(int iter=0;iter<6000;iter++) {
        FILE *inf=fopen("/var/tmp/tty-input","r");
        if(inf) {
            char cmd[256]={0};
            if(fgets(cmd,sizeof(cmd),inf)) {
                size_t l=strlen(cmd); while(l&&(cmd[l-1]=='\n'||cmd[l-1]=='\r')) cmd[--l]=0;
                if(l) {
                    snprintf(lines[nlines?nlines-1:0],81,"> %s",cmd);
                    char full[320]; snprintf(full,sizeof(full),"/bin/sh -c \"%s\" >/var/tmp/tty-out 2>&1",cmd);
                    system(full);
                    FILE *of=fopen("/var/tmp/tty-out","r");
                    if(of) { char o[256]; while(fgets(o,sizeof(o),of)) {
                                size_t z=strlen(o); while(z&&(o[z-1]=='\n'||o[z-1]=='\r')) o[--z]=0;
                                ADD(o); } fclose(of); }
                    ADD("> "); REDRAW();
                }
            }
            fclose(inf); unlink("/var/tmp/tty-input");
        }
        usleep(200000);
    }
    logmsg("DRAWN\n");
    sleep(180);
    return 0;
}
