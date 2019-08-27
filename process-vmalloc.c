/**
 * process-vmalloc.c
 * Capture+output (process) V4L2 device tester.
 *
 * Pawel Osciak, p.osciak@samsung.com
 * 2009, Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <errno.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <stdint.h>

#include <linux/fb.h>
#include <linux/videodev2.h>

#include <sys/mman.h>

#define V4L2_CID_TRANS_TIME_MSEC        (V4L2_CID_PRIVATE_BASE)
#define V4L2_CID_TRANS_NUM_BUFS         (V4L2_CID_PRIVATE_BASE + 1)

#define VIDEO_CAPTURE_DEV_NAME "/dev/video1"

#define VIDEO_DEV_NAME "/dev/video2"
#define FB_DEV_NAME    "/dev/fb0"
#define NUM_BUFS   4
#define NUM_FRAMES 16

#define perror_exit(cond, func)\
   if (cond) {\
       fprintf(stderr, "%s:%d: ", __func__, __LINE__);\
       perror(func);\
       exit(EXIT_FAILURE);\
   }

   #define error_exit(cond, func)\
   if (cond) {\
       fprintf(stderr, "%s:%d: failed\n", func, __LINE__);\
       exit(EXIT_FAILURE);\
   }

   #define perror_ret(cond, func)\
   if (cond) {\
       fprintf(stderr, "%s:%d: ", __func__, __LINE__);\
       perror(func);\
       return ret;\
   }

   #define memzero(x)\
   memset(&(x), 0, sizeof (x));

   #define PROCESS_DEBUG 1
#ifdef PROCESS_DEBUG
#define debug(msg, ...)\
   fprintf(stderr, "%s: " msg, __func__, ##__VA_ARGS__);
   #else
#define debug(msg, ...)
#endif

static int vid_fd, fb_fd, capture_fd;
static void *fb_addr;
static char *p_src_buf[NUM_BUFS], *p_dst_buf[NUM_BUFS];
static size_t src_buf_size[NUM_BUFS], dst_buf_size[NUM_BUFS];
static uint32_t num_src_bufs = 0, num_dst_bufs = 0;

/* Command-line params */
int initial_delay = 0;
int fb_x, fb_y, width, height;
int translen = 1;
/* For displaying multi-buffer transaction simulations, indicates current
 * buffer in an ongoing transaction */
int curr_buf = 0;
int transtime = 1000;
int num_frames = 0;
off_t fb_off, fb_line_w, fb_buf_w;
struct fb_var_screeninfo fbinfo;

static void init_video_dev_capture(void)
{
  struct v4l2_capability cap;
  if((capture_fd = open(VIDEO_CAPTURE_DEV_NAME, O_RDWR)) < 0){
    perror("open");
    exit(1);
  }

    // ...

  if(ioctl(capture_fd, VIDIOC_QUERYCAP, &cap) < 0){
    perror("VIDIOC_QUERYCAP");
    exit(1);
  }
  if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)){
    fprintf(stderr, "The device does not handle single-planar video capture.\n");
    exit(1);
  }
  struct v4l2_format format;
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  format.fmt.pix.width = 800;
  format.fmt.pix.height = 600;

  if(ioctl(capture_fd, VIDIOC_S_FMT, &format) < 0){
    perror("VIDIOC_S_FMT");
    exit(1);
  }
  struct v4l2_requestbuffers bufrequest;
  bufrequest.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bufrequest.memory = V4L2_MEMORY_MMAP;
  bufrequest.count = 1;

  if(ioctl(capture_fd, VIDIOC_REQBUFS, &bufrequest) < 0){
    perror("VIDIOC_REQBUFS");
    exit(1);
  }

  struct v4l2_buffer bufferinfo;
  memset(&bufferinfo, 0, sizeof(bufferinfo));

  bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  bufferinfo.memory = V4L2_MEMORY_MMAP;
  bufferinfo.index = 0;

  if(ioctl(capture_fd, VIDIOC_QUERYBUF, &bufferinfo) < 0){
    perror("VIDIOC_QUERYBUF");
    exit(1);
  }
  void* buffer_start = mmap(
    NULL,
    bufferinfo.length,
    PROT_READ | PROT_WRITE,
    MAP_SHARED,
    capture_fd,
    bufferinfo.m.offset
  );

  if(buffer_start == MAP_FAILED){
    perror("mmap");
    exit(1);
  }

  memset(buffer_start, 0, bufferinfo.length);


memset(&bufferinfo, 0, sizeof(bufferinfo));
bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
bufferinfo.memory = V4L2_MEMORY_MMAP;
bufferinfo.index = 0; /* Queueing buffer index 0. */

// Put the buffer in the incoming queue.
if(ioctl(capture_fd, VIDIOC_QBUF, &bufferinfo) < 0){
    perror("VIDIOC_QBUF");
    exit(1);
}

// Activate streaming
int type = bufferinfo.type;
if(ioctl(capture_fd, VIDIOC_STREAMON, &type) < 0){
    perror("VIDIOC_STREAMON");
    exit(1);
}

// while(/* main loop */){
    // Dequeue the buffer.
    if(ioctl(capture_fd, VIDIOC_DQBUF, &bufferinfo) < 0){
        perror("VIDIOC_QBUF");
        exit(1);
    }

int jpgfile;
if((jpgfile = open("/tmp/myimage.jpeg", O_WRONLY | O_CREAT, 0660)) < 0){
    perror("open");
    exit(1);
}

write(jpgfile, buffer_start, bufferinfo.length);
close(jpgfile);

    bufferinfo.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bufferinfo.memory = V4L2_MEMORY_MMAP;
    /* Set the index if using several buffers */

    // Queue the next one.
    if(ioctl(capture_fd, VIDIOC_QBUF, &bufferinfo) < 0){
        perror("VIDIOC_QBUF");
        exit(1);
    }
// }

// Deactivate streaming
if(ioctl(capture_fd, VIDIOC_STREAMOFF, &type) < 0){
    perror("VIDIOC_STREAMOFF");
    exit(1);
}

  debug("Capture device is OK!");
  close(capture_fd);
  return EXIT_SUCCESS;
}

static void init_video_dev(void)
{
   int ret;
   struct v4l2_capability cap;
   struct v4l2_format fmt;
   struct v4l2_control ctrl;

   vid_fd = open(VIDEO_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
   perror_exit(vid_fd < 0, "open");
/*
   ctrl.id = V4L2_CID_TRANS_TIME_MSEC;
   ctrl.value = transtime;
   ret = ioctl(vid_fd, VIDIOC_S_CTRL, &ctrl);
   perror_exit(ret != 0, "ioctl");

   ctrl.id = V4L2_CID_TRANS_NUM_BUFS;
   ctrl.value = translen;
   ret = ioctl(vid_fd, VIDIOC_S_CTRL, &ctrl);
   perror_exit(ret != 0, "ioctl");

   ret = ioctl(vid_fd, VIDIOC_QUERYCAP, &cap);
   perror_exit(ret != 0, "ioctl");

   if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
       fprintf(stderr, "Device does not support capture 0x%X\n",
                                                              cap.capabilities);
       exit(EXIT_FAILURE);
   }
   if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
       fprintf(stderr, "Device does not support output\n");
       exit(EXIT_FAILURE);
   }
*/
   /* Set format for capture */
   fmt.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   fmt.fmt.pix.width   = width;
   fmt.fmt.pix.height  = height;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565X;
   fmt.fmt.pix.field   = V4L2_FIELD_ANY;

   ret = ioctl(vid_fd, VIDIOC_S_FMT, &fmt);
   perror_exit(ret != 0, "ioctl");

   /* The same format for output */
#if 1
   fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
   fmt.fmt.pix.width   = width;
   fmt.fmt.pix.height  = height;
   fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565X;
   fmt.fmt.pix.field   = V4L2_FIELD_ANY;

   ret = ioctl(vid_fd, VIDIOC_S_FMT, &fmt);
   perror_exit(ret != 0, "ioctl");
#endif
   }

static void gen_src_buf(void *p, size_t size)
{
   uint8_t val;

   val = rand() % 256;
   memset(p, val, size);
   }

static void gen_dst_buf(void *p, size_t size)
{
   /* White */
   memset(p, 255, 0);
   }

static int read_frame(int last)
{
   struct v4l2_buffer buf;
   int ret;
   int j;
   char * p_fb = fb_addr + fb_off;

   memzero(buf);

   buf.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
   buf.memory  = V4L2_MEMORY_MMAP;

   ret = ioctl(vid_fd, VIDIOC_DQBUF, &buf);
   debug("Dequeued source buffer, index: %d\n", buf.index);
   if (ret) {
       switch (errno) {
       case EAGAIN:
           debug("Got EAGAIN\n");
           return 0;

       case EIO:
           debug("Got EIO\n");
           return 0;

       default:
           perror("ioctl");
           return 0;
       }
   }

   /* Verify we've got a correct buffer */
   assert(buf.index < num_src_bufs);

   /* Enqueue back the buffer (note that the index is preserved) */
   if (!last) {
       gen_src_buf(p_src_buf[buf.index], src_buf_size[buf.index]);
       buf.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
       buf.memory  = V4L2_MEMORY_MMAP;
       ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
       perror_ret(ret != 0, "ioctl");
   }


   buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   debug("Dequeuing destination buffer\n");
   ret = ioctl(vid_fd, VIDIOC_DQBUF, &buf);
   if (ret) {
       switch (errno) {
       case EAGAIN:
           debug("Got EAGAIN\n");
           return 0;

       case EIO:
           debug("Got EIO\n");
           return 0;

       default:
           perror("ioctl");
           return 1;
       }
   }
   debug("Dequeued dst buffer, index: %d\n", buf.index);
   /* Verify we've got a correct buffer */
   assert(buf.index < num_dst_bufs);

   debug("Current buffer in the transaction: %d\n", curr_buf);
   p_fb += curr_buf * (height / translen) * fb_line_w;
   ++curr_buf;
   if (curr_buf >= translen)
       curr_buf = 0;

   /* Display results */
   for (j = 0; j < height / translen; ++j) {
       memcpy(p_fb, (void *)p_dst_buf[buf.index], fb_buf_w);
       p_fb += fb_line_w;
   }

   /* Enqueue back the buffer */
   if (!last) {
       gen_dst_buf(p_dst_buf[buf.index], dst_buf_size[buf.index]);
       ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
       perror_ret(ret != 0, "ioctl");
       debug("Enqueued back dst buffer\n");
   }

   return 0;
   }

void init_usage(int argc, char *argv[])
{
   if (argc != 9) {
       printf("Usage: %s initial_delay bufs_per_transaction "
           "trans_length_msec num_frames fb_offset_x fb_offset_y "
           "width height\n", argv[0]);
       exit(EXIT_FAILURE);
   }

   initial_delay = atoi(argv[1]);
   translen = atoi(argv[2]);
   transtime = atoi(argv[3]);
   num_frames = atoi(argv[4]);
   fb_x = atoi(argv[5]);
   fb_y = atoi(argv[6]);
   width = atoi(argv[7]);
   height = atoi(argv[8]);
   debug("NEW PROCESS: fb_x: %d, fb_y: %d, width: %d, height: %d, "
       "translen: %d, transtime: %d, num_frames: %d\n",
       fb_x, fb_y, width, height, translen, transtime, num_frames);
       }

void init_fb(void)
{
   int ret;
   size_t map_size;

   fb_fd = open(FB_DEV_NAME, O_RDWR, 0);
   perror_exit(fb_fd < 0, "open");

   ret = ioctl(fb_fd, FBIOGET_VSCREENINFO, &fbinfo);
   perror_exit(ret != 0, "ioctl");
   debug("fbinfo: xres: %d, xres_virt: %d, yres: %d, yres_virt: %d\n",
       fbinfo.xres, fbinfo.xres_virtual,
       fbinfo.yres, fbinfo.yres_virtual);

   fb_line_w= fbinfo.xres_virtual * (fbinfo.bits_per_pixel >> 3);
   fb_off = fb_y * fb_line_w + fb_x * (fbinfo.bits_per_pixel >> 3);
   fb_buf_w = width * (fbinfo.bits_per_pixel >> 3);
   map_size = fb_line_w * fbinfo.yres_virtual;

   fb_addr = mmap(0, map_size, PROT_WRITE | PROT_READ,
           MAP_SHARED, fb_fd, 0);
   perror_exit(fb_addr == MAP_FAILED, "mmap");
   }

int main(int argc, char *argv[])
{
   int ret = 0;
   int i;
   struct v4l2_buffer buf;
   struct v4l2_requestbuffers reqbuf;
   enum v4l2_buf_type type;
   int last = 0;

   init_usage(argc, argv);
   init_video_dev_capture();
   exit(0);
   init_fb();

   srand(time(NULL) ^ getpid());
   sleep(initial_delay);


   init_video_dev();

   memzero(reqbuf);
   reqbuf.count    = NUM_BUFS;
   reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
   type        = V4L2_BUF_TYPE_VIDEO_OUTPUT;
   reqbuf.memory   = V4L2_MEMORY_MMAP;
   ret = ioctl(vid_fd, VIDIOC_REQBUFS, &reqbuf);
   perror_exit(ret != 0, "ioctl");
   num_src_bufs = reqbuf.count;
   debug("Got %d src buffers\n", num_src_bufs);

   reqbuf.count    = NUM_BUFS;
   reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
   ret = ioctl(vid_fd, VIDIOC_REQBUFS, &reqbuf);
   perror_exit(ret != 0, "ioctl");
   num_dst_bufs = reqbuf.count;
   debug("Got %d dst buffers\n", num_dst_bufs);

   for (i = 0; i < num_src_bufs; ++i) {
       buf.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
       buf.memory  = V4L2_MEMORY_MMAP;
       buf.index   = i;

       ret = ioctl(vid_fd, VIDIOC_QUERYBUF, &buf);
       perror_exit(ret != 0, "ioctl");
       debug("QUERYBUF returned offset: %x\n", buf.m.offset);

       src_buf_size[i] = buf.length;
       p_src_buf[i] = mmap(NULL, buf.length,
                   PROT_READ | PROT_WRITE, MAP_SHARED,
                   vid_fd, buf.m.offset);
       perror_exit(MAP_FAILED == p_src_buf[i], "mmap");
   }

   for (i = 0; i < num_dst_bufs; ++i) {
       buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       buf.memory  = V4L2_MEMORY_MMAP;
       buf.index   = i;

       ret = ioctl(vid_fd, VIDIOC_QUERYBUF, &buf);
       perror_exit(ret != 0, "ioctl");
       debug("QUERYBUF returned offset: %x\n", buf.m.offset);

       dst_buf_size[i] = buf.length;
       p_dst_buf[i] = mmap(NULL, buf.length,
                   PROT_READ | PROT_WRITE, MAP_SHARED,
                   vid_fd, buf.m.offset);
       perror_exit(MAP_FAILED == p_dst_buf[i], "mmap");
   }

   for (i = 0; i < num_src_bufs; ++i) {

       gen_src_buf(p_src_buf[i], src_buf_size[i]);

       memzero(buf);
       buf.type    = V4L2_BUF_TYPE_VIDEO_OUTPUT;
       buf.memory  = V4L2_MEMORY_MMAP;
       buf.index   = i;

       ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
       perror_exit(ret != 0, "ioctl");
   }

   for (i = 0; i < num_dst_bufs; ++i) {
       memzero(buf);
       buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
       buf.memory  = V4L2_MEMORY_MMAP;
       buf.index   = i;

       ret = ioctl(vid_fd, VIDIOC_QBUF, &buf);
       perror_exit(ret != 0, "ioctl");
   }

   ret = ioctl(vid_fd, VIDIOC_STREAMON, &type);
   debug("STREAMON (%d): %d\n", VIDIOC_STREAMON, ret);
   perror_exit(ret != 0, "ioctl");

   while (num_frames) {
       fd_set read_fds;
       int r;

       FD_ZERO(&read_fds);
       FD_SET(vid_fd, &read_fds);

       debug("Before select");
       r = select(vid_fd + 1, &read_fds, NULL, NULL, 0);
       perror_exit(r < 0, "select");
       debug("After select");

       if (num_frames == 1)
           last = 1;
       if (read_frame(last)) {
           fprintf(stderr, "Read frame failed\n");
           break;
       }
       --num_frames;
       printf("FRAMES LEFT: %d\n", num_frames);
   }


   done:
   close(vid_fd);
   close(fb_fd);

   for (i = 0; i < num_src_bufs; ++i)
       munmap(p_src_buf[i], src_buf_size[i]);

   for (i = 0; i < num_dst_bufs; ++i)
       munmap(p_dst_buf[i], dst_buf_size[i]);

   return ret;
}


