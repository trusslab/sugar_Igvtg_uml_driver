/*
 * Copyright (C) 2016-2018 University of California, Irvine
 * All Rights Reserved.
 *
 * Authors:
 * Zhihao Yao <z.yao@uci.edu>
 * Ardalan Amiri Sani <arrdalan@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _PRINTS_H_
#define _PRINTS_H_

/* #define I915_VGT_ISOL_DEBUG */
/* #define I915_VGT_ISOL_TWO_GPU */

#define PRINTK0(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK1(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK2(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK3(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK4(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK5(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK6(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK7(fmt, args...) /* printk("%s: " fmt, __func__, ##args); */
#define PRINTK_MUST(fmt, args...)  printk("%s: " fmt, __func__, ##args);
#define PRINTK_ERR(fmt, args...) printk("Error: %s: " fmt, __func__, ##args)

#endif /* _PRINTS_H_ */
