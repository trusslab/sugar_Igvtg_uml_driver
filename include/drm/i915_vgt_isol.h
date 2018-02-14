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

#ifndef _I915_VGT_ISOL_
#define _I915_VGT_ISOL_

#include <linux/dma-buf.h>
#include <linux/prints.h>

void destroy_vgt_instance(void);

void __iomem *vgt_isol_pci_iomap(void *dev, int bar, unsigned long maxlen);
void __iomem *vgt_isol_ioremap_nocache(resource_size_t res_cookie, size_t size);
void __iomem *vgt_isol_ioremap_wc(resource_size_t res_cookie, size_t size);

int vgt_isol_pci_read_config(int size, int where, int *val);
int vgt_isol_pci_write_config(int size, int where, int val);
int vgt_isol_read_mmio(int size, unsigned long where, void *val);
int vgt_isol_write_mmio(int size, unsigned long where, unsigned long val);

extern unsigned long PCI_CONFIG_BASE_ADDR;

static inline int vgt_isol_pci_read_config_byte(const struct pci_dev *dev, int where, u8 *val)
{
#ifdef VGT_ISOL_PCI_RDWR_SYSFS
	int tmp_val;
	int ret;

	ret = vgt_isol_pci_read_config(8, where, &tmp_val);

	*val = *((u8 *) (&tmp_val));

	return ret;
#else /* VGT_ISOL_PCI_RDWR_SYSFS */
	*val = readb((void __iomem *) (PCI_CONFIG_BASE_ADDR + where)); 
	return 0;
#endif /* VGT_ISOL_PCI_RDWR_SYSFS */
}

static inline int vgt_isol_pci_read_config_word(const struct pci_dev *dev, int where, u16 *val)
{
#ifdef VGT_ISOL_PCI_RDWR_SYSFS
	int tmp_val;
	int ret;

	ret = vgt_isol_pci_read_config(16, where, &tmp_val);

	*val = *((u16 *) (&tmp_val));

	return ret;
#else /* VGT_ISOL_PCI_RDWR_SYSFS */
	*val = readw((void __iomem *) (PCI_CONFIG_BASE_ADDR + where)); 
	return 0;
#endif /* VGT_ISOL_PCI_RDWR_SYSFS */
}

static inline int vgt_isol_pci_read_config_dword(const struct pci_dev *dev, int where,
					u32 *val)
{
#ifdef VGT_ISOL_PCI_RDWR_SYSFS
	int tmp_val;
	int ret;

	ret = vgt_isol_pci_read_config(32, where, &tmp_val);

	*val = *((u32 *) (&tmp_val));

	return ret;
#else /* VGT_ISOL_PCI_RDWR_SYSFS */
	*val = readl((void __iomem *) (PCI_CONFIG_BASE_ADDR + where)); 
	return 0;
#endif /* VGT_ISOL_PCI_RDWR_SYSFS */
}

static inline int vgt_isol_pci_write_config_byte(const struct pci_dev *dev, int where, u8 val)
{
#ifdef VGT_ISOL_PCI_RDWR_SYSFS
	return vgt_isol_pci_write_config(8, where, (int) val);
#else /* VGT_ISOL_PCI_RDWR_SYSFS */
	writeb(val, (void __iomem *) (PCI_CONFIG_BASE_ADDR + where)); 
	return 0;
#endif /* VGT_ISOL_PCI_RDWR_SYSFS */
}

static inline int vgt_isol_pci_write_config_word(const struct pci_dev *dev, int where, u16 val)
{
#ifdef VGT_ISOL_PCI_RDWR_SYSFS
	return vgt_isol_pci_write_config(16, where, (int) val);
#else /* VGT_ISOL_PCI_RDWR_SYSFS */
	writew(val, (void __iomem *) (PCI_CONFIG_BASE_ADDR + where)); 
	return 0;
#endif /* VGT_ISOL_PCI_RDWR_SYSFS */
}

static inline int vgt_isol_pci_write_config_dword(const struct pci_dev *dev, int where,
					 u32 val)
{
#ifdef VGT_ISOL_PCI_RDWR_SYSFS
	return vgt_isol_pci_write_config(32, where, (int) val);
#else /* VGT_ISOL_PCI_RDWR_SYSFS */
	writel(val, (void __iomem *) (PCI_CONFIG_BASE_ADDR + where)); 
	return 0;
#endif /* VGT_ISOL_PCI_RDWR_SYSFS */
}

static DEFINE_MUTEX(vgt_isol_mmio_mutex);
#define ENTER_CRIT_SEC mutex_lock(&vgt_isol_mmio_mutex)
#define EXIT_CRIT_SEC mutex_unlock(&vgt_isol_mmio_mutex)

static inline u8 vgt_isol_readb(const volatile void __iomem *addr)
{
	unsigned long val = 0;
	int ret;

	ENTER_CRIT_SEC;

	ret = vgt_isol_read_mmio(8, (unsigned long) addr, &val);
	if (ret)
		PRINTK_ERR("Error: vgt_isol_read failed\n");
	
	EXIT_CRIT_SEC;

	return (u8) val;
}

static inline u16 vgt_isol_readw(const volatile void __iomem *addr)
{
	unsigned long val = 0;
	int ret;

	ENTER_CRIT_SEC;

	ret = vgt_isol_read_mmio(16, (unsigned long) addr, &val);
	if (ret)
		PRINTK_ERR("Error: vgt_isol_read failed\n");
	
	EXIT_CRIT_SEC;

	return (u16) val;
}

static inline u32 vgt_isol_readl(const volatile void __iomem *addr)
{
	unsigned long val = 0;
	int ret;

	ENTER_CRIT_SEC;

	ret = vgt_isol_read_mmio(32, (unsigned long) addr, &val);
	if (ret)
		PRINTK_ERR("Error: vgt_isol_read failed\n");
	
	EXIT_CRIT_SEC;

	return (u32) val;
}

#ifdef CONFIG_64BIT
static inline u64 vgt_isol_readq(const volatile void __iomem *addr)
{
	unsigned long val = 0;
	int ret;

	ENTER_CRIT_SEC;

	ret = vgt_isol_read_mmio(64, (unsigned long) addr, &val);
	if (ret)
		PRINTK_ERR("Error: vgt_isol_read failed\n");
	
	EXIT_CRIT_SEC;

	return (u64) val;
}
#endif /* CONFIG_64BIT */

static inline void vgt_isol_writeb(u8 value, volatile void __iomem *addr)
{
	ENTER_CRIT_SEC;
	vgt_isol_write_mmio(8, (unsigned long) addr, (unsigned long) value);
	EXIT_CRIT_SEC;
}

static inline void vgt_isol_writew(u16 value, volatile void __iomem *addr)
{
	ENTER_CRIT_SEC;
	vgt_isol_write_mmio(16, (unsigned long) addr, (unsigned long) value);
	EXIT_CRIT_SEC;
}

static inline void vgt_isol_writel(u32 value, volatile void __iomem *addr)
{
	ENTER_CRIT_SEC;
	vgt_isol_write_mmio(32, (unsigned long) addr, (unsigned long) value);
	EXIT_CRIT_SEC;
}

#ifdef CONFIG_64BIT
static inline void vgt_isol_writeq(u64 value, volatile void __iomem *addr)
{
	ENTER_CRIT_SEC;
	vgt_isol_write_mmio(64, (unsigned long) addr, (unsigned long) value);
	EXIT_CRIT_SEC;
}
#endif /* CONFIG_64BIT */

struct vgt_isol_shmem_struct {
	void *vaddr;
	int num_pages;
	loff_t size;
	struct file *file;
};

struct file *vgt_isol_shmem_file_setup(const char *name, loff_t size,
							unsigned long flags);

struct page *vgt_isol_shmem_read_mapping_page_gfp(struct address_space *mapping,
					 pgoff_t index, gfp_t gfp);

struct page *vgt_isol_shmem_read_mapping_page(
				struct address_space *mapping, pgoff_t index);

void vgt_isol_shmem_truncate_range(struct inode *inode, loff_t lstart, loff_t lend);

void *vgt_isol_shmem_kmap_page(struct drm_gem_object *drm_obj, int nth_page);

struct dma_buf *vgt_isol_dma_buf_export(const struct dma_buf_export_info *exp_info);
int vgt_isol_dma_buf_fd(struct dma_buf *dmabuf, int flags);
#endif/* _I915_VGT_ISOL_ */
