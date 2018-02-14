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

#include <drm/drmP.h>
#include <linux/module.h> 
#include <linux/prints.h>
#include <drm/i915_vgt_isol.h>
#include <linux/pagemap.h>
#include "vgt.h"

struct list_head files_list;
struct list_head vma_list;

int vgt_isol_init(void)
{
	int ret = 0;
	
	INIT_LIST_HEAD(&files_list);
	INIT_LIST_HEAD(&vma_list);

	return ret;
}

/* syscalls */

struct file_entry {
	struct file file;
	int fd;
	struct list_head list;
};

extern const struct file_operations i915_driver_fops;

static struct file *add_file_entry(int fd)
{
	struct file_entry *entry = NULL;
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		PRINTK_ERR("Error: could not allocate memory\n");
		return NULL;
	}
	
	entry->fd = fd;
	entry->file.f_op = &i915_driver_fops;
		
	INIT_LIST_HEAD(&entry->list);
	list_add(&entry->list, &files_list);

	return &entry->file;
}

static struct file *get_file_entry(int fd)
{
	struct file_entry *entry = NULL, *tmp;

	list_for_each_entry_safe(entry, tmp, &files_list, list)
	{
		if (entry->fd == fd)
			return &entry->file;
	}

	return NULL;
}

/* Complete and call it at remove file operation */
static int remove_file_entry(int fd)
{
	struct file_entry *entry = NULL, *tmp;

	list_for_each_entry_safe(entry, tmp, &files_list, list)
	{
		if (entry->fd == fd) {
			list_del(&entry->list);
			kfree(entry);
		}
	}

	return 0;
	
}

int isol_open_syscall(const char *pathname, int flags, int mode, int fd)
{
	int ret;
	struct file *file;
	
	remove_file_entry(fd); 
	
	file = add_file_entry(fd);

	if (file == NULL) {
		PRINTK_ERR("Error: file is NULL\n");
		return -EINVAL;
	}
	
	ret = drm_open(NULL, file);
		
	return ret;
}

int isol_ioctl_syscall(int fd, unsigned long cmd, void *arg)
{
	struct file *file;
	
	file = get_file_entry(fd);
	if (file == NULL) {
		PRINTK_ERR("Error: file is NULL\n");
		return -EINVAL;
	}
	
	return (int) drm_ioctl(file, cmd, (unsigned long) arg);	
}

struct vma_entry {
	struct vm_area_struct vma;
	void *data;	
	struct list_head list;
};

static struct vma_entry *add_vma_entry(unsigned long addr,
							unsigned long length)
{
	struct vma_entry *entry = NULL;
	
	entry = kmalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		PRINTK_ERR("Error: could not allocate memory\n");
		return NULL;
	}
			
	INIT_LIST_HEAD(&entry->list);
	list_add(&entry->list, &vma_list);
	
	entry->vma.vm_start = addr;
	entry->vma.vm_end = addr + length;

	return entry;
}

static struct vma_entry *get_vma_entry(unsigned long addr,
							unsigned long length)
{
	struct vma_entry *entry = NULL, *tmp;

	list_for_each_entry_safe(entry, tmp, &vma_list, list)
	{
		if (entry->vma.vm_start <= addr &&
		    entry->vma.vm_end >= (addr + length))
			return entry;
	}

	return NULL;
}

static int remove_vma_entry(struct vm_area_struct *vma)
{
	struct vma_entry *entry = NULL, *tmp;
	int counter = 0;

	list_for_each_entry_safe(entry, tmp, &vma_list, list)
	{
		counter++;
		if (&entry->vma == vma) {
			list_del(&entry->list);
			kfree(entry);
		}
	}
	
	return 0;	
}

int drm_gem_mmap(struct file *filp, struct vm_area_struct *vma);

void *isol_mmap_syscall(void *addr, size_t length, int prot, int flags,
	int fd, off_t offset)
{
	void *ret_addr;
	struct file *file;
	int ret;
	struct vma_entry *vm_entry;
	struct vm_area_struct *vma;
	struct vm_fault vmf;
	
	file = get_file_entry(fd);
	if (file == NULL) {
		PRINTK_ERR("Error: file is NULL\n");
		return NULL;
	}
	
	vm_entry = add_vma_entry(0, 0);
	
	if (!vm_entry) {
		PRINTK_ERR("Error: could not add vma_entry\n");
		return NULL;
	}
	vma = &vm_entry->vma;

	vma->vm_pgoff = offset >> PAGE_SHIFT;
	
	ret = drm_gem_mmap(file, vma);

	vma->vm_ops->open(vma);

	vma->vm_start = 0;
	vma->vm_end = length - 1;
	vmf.virtual_address = 0;
	ret = vma->vm_ops->fault(vma, &vmf);

	ret_addr = vmf.virtual_address;
	vma->vm_start = (unsigned long) ret_addr;
	vma->vm_end = (unsigned long) ret_addr + length - 1;

	return ret_addr;	
}

int isol_munmap_syscall(void *addr, size_t length)
{
	struct vma_entry *vm_entry;
	struct vm_area_struct *vma;
	int ret;
	
	vm_entry = get_vma_entry((unsigned long) addr, (unsigned long) length);
	if (!vm_entry) {
		PRINTK_ERR("Error: could not find vm_entry\n");
		return -EINVAL;
	}
	vma = &vm_entry->vma;
	
	BUG();
	
	remove_vma_entry(vma);
	
	return ret;
}

ssize_t isol_read_syscall(int fd, void *buf, size_t nbyte)
{
	struct file *file;
	
	file = get_file_entry(fd);
	if (file == NULL) {
		PRINTK_ERR("Error: file is NULL\n");
		return -EINVAL;
	}
	
	/* setting offset to 0 */
	return drm_read(file, (char __user *) buf, nbyte, 0);		
}

int isol_poll_syscall(int fd, int timeout)
{
	struct file *file;
	
	file = get_file_entry(fd);
	if (file == NULL) {
		PRINTK_ERR("Error: file is NULL\n");
		return -EINVAL;
	}
	
	return drm_poll(file, NULL);		
}

void __iomem *vgt_isol_pci_iomap(void *dev, int bar, unsigned long maxlen)
{
	unsigned long addr = 0x0;

	if (bar == 0) {
		vgt_isol_pci_read_config_dword(NULL, VGT_REG_CFG_SPACE_BAR0, (u32 *) &addr);
	} else {
		BUG();
	}
	
	#define PCI_BAR_ADDR_MASK (~0xFUL)  /* 4 LSB bits are not address */
	return (void __iomem *) (addr & PCI_BAR_ADDR_MASK);
}

void __iomem *vgt_isol_ioremap_nocache(phys_addr_t offset, size_t size)
{
	u32 base_addr;
	
	vgt_isol_pci_read_config_dword(NULL, VGT_REG_CFG_SPACE_BAR0, (u32 *) &base_addr);

	return (void __iomem *) ((base_addr & PCI_BAR_ADDR_MASK) + offset);
}

void __iomem *vgt_isol_ioremap_wc(phys_addr_t offset, size_t size)
{
	u32 base_addr;

	vgt_isol_pci_read_config_dword(NULL, VGT_REG_CFG_SPACE_BAR0, (u32 *) &base_addr);

	return (void __iomem *) ((base_addr & PCI_BAR_ADDR_MASK) + offset);
}

static int vgt_isol_shmem_mmap(struct file *file, struct vm_area_struct *vma)
{
	return 0;
}

static unsigned long vgt_isol_shmem_get_unmapped_area(struct file *filp,
			unsigned long addr, unsigned long len,
			unsigned long pgoff, unsigned long flags)
{
	struct vgt_isol_shmem_struct *info =
					filp->f_inode->i_mapping->private_data;
	
	return (unsigned long) info->vaddr;
}

static const struct file_operations vgt_isol_shmem_file_operations = {
	.mmap = vgt_isol_shmem_mmap,
	.get_unmapped_area = vgt_isol_shmem_get_unmapped_area,
};

void *um_malloc(size_t size);
void um_free(const void *addr, size_t size);
int um_pin_memory(unsigned long start_page, unsigned long num_pages);
int um_unpin_memory(unsigned long start_page, unsigned long num_pages);

#define NATIVE_ALLOC_MEM_LIMIT		4096

struct file *vgt_isol_shmem_file_setup(const char *name, loff_t size,
							unsigned long flags)
{
	struct file *file;
	int num_pages;
	void *vaddr;
	struct vgt_isol_shmem_struct *info;
	
	num_pages = size / PAGE_SIZE;
	
	if (size != (num_pages * PAGE_SIZE)) {
		PRINTK_ERR("Error: size should be exact number of pages\n");
		return NULL;
	}
		
	if (size <= NATIVE_ALLOC_MEM_LIMIT)
		vaddr = kmalloc(size, GFP_KERNEL);
	else
		vaddr = um_malloc(size);
	if (!vaddr) {
		PRINTK_ERR("Error: vgt_malloc failed\n");
		return NULL;
	}
	
	um_pin_memory(((unsigned long) vaddr) >> PAGE_SHIFT, ((unsigned long) size) >> PAGE_SHIFT);

	file = kmalloc(sizeof(*file), GFP_KERNEL);
	if (!file) {
		PRINTK_ERR("Error: could not allocate memory for file\n");
		return NULL;
	}

	file->f_op = &vgt_isol_shmem_file_operations;
	
	file->f_mode |= FMODE_READ;
	file->f_mode |= FMODE_WRITE;

	file->f_inode = kmalloc(sizeof(*file->f_inode), GFP_KERNEL);
	if (!file) {
		PRINTK_ERR("Error: could not allocate memory for file->f_inode\n");
		return NULL;
	}

	file->f_inode->i_mapping = kmalloc(sizeof(*file->f_inode->i_mapping), GFP_KERNEL);
	if (!file) {
		PRINTK_ERR("Error: could not allocate memory for file->f_inode->i_mapping\n");
		return NULL;
	}
	
	info = kmalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		PRINTK_ERR("Error: could not allocate memory for info\n");
		kfree(file);
		return NULL;
	}
	
	info->vaddr = vaddr;
	info->num_pages = num_pages;
	info->size = size; 
	info->file = file; 
	file->f_inode->i_mapping->private_data = (void *) info;
	
	return file;
}

struct page *vgt_isol_shmem_read_mapping_page_gfp(struct address_space *mapping,
					 pgoff_t index, gfp_t gfp)
{
	struct vgt_isol_shmem_struct *info = mapping->private_data;
	void *page_vaddr;
	struct page *page;
	
	if (index >= info->num_pages) {
		PRINTK_ERR("Error: invalid requested page number %d, total "
			   "number of pages is %d\n", (int) index, (int) info->num_pages);
		return NULL;
	}

	page_vaddr = info->vaddr + (index * PAGE_SIZE);

	page = virt_to_page(page_vaddr);

	return page;	
}

struct page *vgt_isol_shmem_read_mapping_page(
				struct address_space *mapping, pgoff_t index)
{
	return vgt_isol_shmem_read_mapping_page_gfp(mapping, index,
					mapping_gfp_mask(mapping));
}

void vgt_isol_shmem_truncate_range(struct inode *inode, loff_t lstart, loff_t lend)
{
	void *vaddr;
	struct vgt_isol_shmem_struct *info;
	
	if ((lstart != 0) || (lend != ((loff_t) - 1))) /* we don't support partial truncation */
		BUG();

	info = inode->i_mapping->private_data;
	vaddr = info->vaddr;

	um_unpin_memory(((unsigned long) vaddr) >> PAGE_SHIFT,
			((unsigned long) info->size) >> PAGE_SHIFT);

	if (info->size <= NATIVE_ALLOC_MEM_LIMIT)
		kfree(vaddr);
	else
		um_free(vaddr, info->size);
	kfree(inode->i_mapping);
	kfree(inode);
	kfree(info->file);
	kfree(info);
}

void *vgt_isol_shmem_kmap_page(struct drm_gem_object *drm_obj, int nth_page)
{
	struct file *filp = drm_obj->filp;
	struct vgt_isol_shmem_struct *info = filp->f_inode->i_mapping->private_data;
	void *addr = info->vaddr + (nth_page * PAGE_SIZE);

	return addr;
}

int um_dma_buf_export(unsigned long start_page, unsigned long num_pages, int flags);

struct dma_buf *vgt_isol_dma_buf_export(const struct dma_buf_export_info *exp_info)
{
	struct drm_gem_object *drm_obj = (struct drm_gem_object *) exp_info->priv;
	struct vgt_isol_shmem_struct *info;
	int ret;
	struct dma_buf *dmabuf;
	
	info = (struct vgt_isol_shmem_struct *)
				drm_obj->filp->f_inode->i_mapping->private_data;
	
	if (exp_info->size != (info->num_pages * PAGE_SIZE)) {
		PRINTK_ERR("Error: unexpected size\n");
		BUG();
		return NULL;
	}
	
	dmabuf = dma_buf_export(exp_info);
	
	ret = um_dma_buf_export((unsigned long) info->vaddr >> PAGE_SHIFT,
			       (unsigned long) info->num_pages, (int) exp_info->flags);
	if (ret < 0) {
		PRINTK_ERR("Error: um_dma_buf_export() failed\n");
		BUG();
		return NULL;
	}
	
	dmabuf->vgt_fd = ret;

	return dmabuf;
}

int vgt_isol_dma_buf_fd(struct dma_buf *dmabuf, int flags)
{
	return dmabuf->vgt_fd;
}

struct pci_dev *g_pdev = NULL;

void vgt_isol_handle_signal(void)
{
	struct drm_device *drm_dev = NULL;

	if (!g_pdev) {
		return;
	}
	
	drm_dev = pci_get_drvdata(g_pdev);
	
	if (drm_dev)
		drm_dev->driver->irq_handler(0, drm_dev);
	
}
