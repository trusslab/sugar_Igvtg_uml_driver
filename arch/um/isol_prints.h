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

double get_time(void);

#define PRINTF0(fmt, args...) /* fprintf(stderr, "[%f] isol-user: %s: " fmt, get_time(), __func__, ##args) */
#define PRINTF1(fmt, args...) /* fprintf(stderr, "[%f] isol-user: %s: " fmt, get_time(), __func__, ##args) */
#define PRINTF2(fmt, args...) /* fprintf(stderr, "[%f] isol-user: %s: " fmt, get_time(), __func__, ##args) */
#define PRINTF3(fmt, args...) /* fprintf(stderr, "[%f] isol-user: %s: " fmt, get_time(), __func__, ##args) */
#define PRINTF_COND0(cond, fmt, args...) /* {if (cond) fprintf(stderr, "[%f] isol-user: %s: " fmt, get_time(), __func__, ##args);} */
#define PRINTF_ERR(fmt, args...) fprintf(stderr, "[%f] isol-user: %s: " fmt, get_time(), __func__, ##args)
#define PRINTK_STUB(fmt) fprintf(stderr, "[%f] %s", get_time(), fmt)

#define INTEL_GPU_DEV_FILE_NAME		"/dev/dri/card0"
