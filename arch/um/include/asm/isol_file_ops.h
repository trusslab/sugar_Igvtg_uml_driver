/*
 * Copyright (C) 2016-2018 University of California, Irvine
 * All Rights Reserved.
 *
 * Authors:
 * Zhihao Yao <z.yao@uci.edu>
 * Ardalan Amiri Sani <arrdalan@gmail.com>
 *
 * Licensed under the GPL
 */
#ifndef _ISOL_FILE_OPS_H_
#define _ISOL_FILE_OPS_H_

int isol_open(const char *pathname, int flags, mode_t mode);
int isol_ioctl(int fd, unsigned long cmd, void *arg);
void *isol_mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
int isol_munmap(void *addr, size_t length);
size_t isol_read(int fd, void *buf, size_t nbyte);
int isol_poll(int fd, int timeout);
int isol_close(int fd);

#endif /* _ISOL_FILE_OPS_H_ */
