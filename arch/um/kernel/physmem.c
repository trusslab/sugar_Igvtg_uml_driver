/*
 * Copyright (C) 2000 - 2007 Jeff Dike (jdike@{addtoit,linux.intel}.com)
 * Licensed under the GPL
 */

#include <linux/module.h>
#include <linux/bootmem.h>
#include <linux/mm.h>
#include <linux/pfn.h>
#include <asm/page.h>
#include <asm/sections.h>
#include <as-layout.h>
#include <init.h>
#include <kern.h>
#include <mem_user.h>
#include <os.h>
#include <asm/io.h>
#include <linux/prints.h>
#include <linux/pci.h>
#include <linux/pci_ids.h>
#include <linux/scatterlist.h>

static int physmem_fd = -1;

/* Changed during early boot */
unsigned long high_physmem;
EXPORT_SYMBOL(high_physmem);

extern unsigned long long physmem_size;

void __init mem_total_pages(unsigned long physmem, unsigned long iomem,
		     unsigned long highmem)
{
	unsigned long phys_pages, highmem_pages;
	unsigned long iomem_pages, total_pages;

	phys_pages    = physmem >> PAGE_SHIFT;
	iomem_pages   = iomem   >> PAGE_SHIFT;
	highmem_pages = highmem >> PAGE_SHIFT;

	total_pages   = phys_pages + iomem_pages + highmem_pages;

	max_mapnr = total_pages;
}

#define _IOMMUF_readable 0
#define IOMMUF_readable  (1u<<_IOMMUF_readable)
#define _IOMMUF_writable 1
#define IOMMUF_writable  (1u<<_IOMMUF_writable)

int um_map_in_iommu(void *vaddr)
{
	BUG();

}

int um_pin_memory(unsigned long start_page, unsigned long num_pages);
int um_unpin_memory(unsigned long start_page, unsigned long num_pages);

void __iomem *ioremap(phys_addr_t offset, unsigned long size)
{
		return (offset + 0xd0000000);
}

void iounmap(void *addr)
{
}

int
dma_map_sg(void *dev, struct scatterlist *sg, int nents,
           int direction)
{
	int i, ents;
	struct scatterlist *s;
	struct page *page;
	
	for_each_sg(sg, s, nents, i) {
		/*
		 * To see where the ~0x3 comes from, take a look at the
		 * sg_assign_page() in include/linux/scatterlist.h.
		 */
	        page = (dma_addr_t) sg->page_link & ~0x3; 
	        sg->dma_address = __va(page_to_phys(page)); 
		um_pin_memory((unsigned long) sg->dma_address >> PAGE_SHIFT,
			      (unsigned long) sg_dma_len(sg) >> PAGE_SHIFT);
	}

	return nents; /* 0 is error */
}

void
dma_unmap_sg(void *dev, struct scatterlist *sg, int nhwentries,
             int direction)
{
	int i;
	struct scatterlist *s;
	struct page *page;
	
	for_each_sg(sg, s, nhwentries, i) {
		/*
		 * To see where the ~0x3 comes from, take a look at the
		 * sg_assign_page() in include/linux/scatterlist.h.
		 */
	        page = (dma_addr_t) sg->page_link & ~0x3; 
	        sg->dma_address = __va(page_to_phys(page)); 
		um_unpin_memory((unsigned long) sg->dma_address >> PAGE_SHIFT,
			      (unsigned long) sg_dma_len(sg) >> PAGE_SHIFT);
	}
}

int hdmi_spd_infoframe_init(void *frame,
                             const char *vendor, const char *product)
{
	return 0;
}

void dma_free_coherent(struct device *dev, size_t size,
                    void *vaddr, dma_addr_t bus)
{
	BUG();
}

int hdmi_vendor_infoframe_init(void *frame)
{
	return 0;
}

dma_addr_t
dma_map_page(void *dev, struct page *page, unsigned long offset,
             size_t size, int direction)
{
	dma_addr_t addr = (dma_addr_t) __va(page_to_pfn(page) << PAGE_SHIFT);

	if ((size != PAGE_SIZE) || offset)
		PRINTK_ERR("Error: unexpected size or offset!\n");
	um_pin_memory((unsigned long) addr >> PAGE_SHIFT, 1);
	return addr;
}

void
dma_unmap_page(void *dev, dma_addr_t dma_address, size_t size,
               int direction)
{
	if (size != PAGE_SIZE)
		PRINTK_ERR("Error: unexpected size!\n");

	um_unpin_memory((unsigned long) dma_address >> PAGE_SHIFT, 1);
}

int set_pages_wb(struct page *page, int numpages)
{
	return 0;
}

int set_pages_uc(struct page *page, int numpages)
{
	return 0;
}

int hdmi_avi_infoframe_init(void *frame)
{
	return 0;
}

int dma_mapping_error(void *dev, dma_addr_t dma_addr)
{
	return 0;
}

ssize_t
hdmi_infoframe_pack(void *frame, void *buffer, size_t size)
{
	return 0;
}

extern void *
dma_alloc_coherent(void *dev, size_t size, dma_addr_t *dma_handle,
                   gfp_t flag)
{
	return 0x1;
}

int fb_get_options(const char *name, char **option)
{
	return 0;
}

int cn_netlink_send(void *msg, u32 portid, u32 group, gfp_t gfp_mask)
{
	return 0;
}

int pci_bus_alloc_resource(void *bus, void *res,
                 resource_size_t size, resource_size_t align,
                 resource_size_t min, unsigned long type_mask,
                 resource_size_t (*alignf)(void *,
                                           const void *,
                                           resource_size_t,
                                           resource_size_t),
                 void *alignf_data)
{
	return 0;
}

extern void *vgt_local_opregion_va;

#define VGT_OPREGION_SIZE 0x2000 /* two pages for opregion */
/* from drivers/gpu/drm/i915/vgt/vbios.h */
#define VBIOS_OFFSET 0x400

void __iomem *pci_map_rom(void *pdev, size_t *size)
{
	*size = (VGT_OPREGION_SIZE - VBIOS_OFFSET); /* two pages */ 
	return (void __iomem *) (vgt_local_opregion_va + VBIOS_OFFSET);
}

void pci_unmap_rom(void *pdev, void __iomem *rom)
{
}

static struct pci_dev pdev;
/* Copied from drivers/gpu/drm/i915/i915_drv.h */
#define INTEL_PCH_LPT_DEVICE_ID_TYPE		0x8c00

struct pci_dev *pci_get_class(unsigned int class, struct pci_dev *from)
{
	if (from == NULL) {
		pdev.vendor = PCI_VENDOR_ID_INTEL;
		pdev.device = INTEL_PCH_LPT_DEVICE_ID_TYPE;
		return &pdev;
	} else {
		return NULL;
	}
}

int pci_enable_msi_block(void *dev, unsigned int nvec)
{

	((struct pci_dev *) dev)->irq = 10;

	return 0;
}

void pci_disable_msi(void *dev)
{
}

int dma_supported(struct device *dev, u64 mask)
{
}

const char *fb_mode_option;
EXPORT_SYMBOL_GPL(fb_mode_option);

void map_memory(unsigned long virt, unsigned long phys, unsigned long len,
		int r, int w, int x)
{
	__u64 offset;
	int fd, err;

	fd = phys_mapping(phys, &offset);
	err = os_map_memory((void *) virt, fd, offset, len, r, w, x);
	if (err) {
		if (err == -ENOMEM)
			printk(KERN_ERR "try increasing the host's "
			       "/proc/sys/vm/max_map_count to <physical "
			       "memory size>/4096\n");
		panic("map_memory(0x%lx, %d, 0x%llx, %ld, %d, %d, %d) failed, "
		      "err = %d\n", virt, fd, offset, len, r, w, x, err);
	}
}

/**
 * setup_physmem() - Setup physical memory for UML
 * @start:	Start address of the physical kernel memory,
 *		i.e start address of the executable image.
 * @reserve_end:	end address of the physical kernel memory.
 * @len:	Length of total physical memory that should be mapped/made
 *		available, in bytes.
 * @highmem:	Number of highmem bytes that should be mapped/made available.
 *
 * Creates an unlinked temporary file of size (len + highmem) and memory maps
 * it on the last executable image address (uml_reserved).
 *
 * The offset is needed as the length of the total physical memory
 * (len + highmem) includes the size of the memory used be the executable image,
 * but the mapped-to address is the last address of the executable image
 * (uml_reserved == end address of executable image).
 *
 * The memory mapped memory of the temporary file is used as backing memory
 * of all user space processes/kernel tasks.
 */
void __init setup_physmem(unsigned long start, unsigned long reserve_end,
			  unsigned long len, unsigned long long highmem)
{
	unsigned long reserve = reserve_end - start;
	unsigned long pfn = PFN_UP(__pa(reserve_end));
	unsigned long delta = (len - reserve) >> PAGE_SHIFT;
	unsigned long offset, bootmap_size;
	long map_size;
	int err;

	offset = uml_reserved - uml_physmem;
	map_size = len - offset;
	if(map_size <= 0) {
		printf("Too few physical memory! Needed=%d, given=%d\n",
		       offset, len);
		exit(1);
	}

	physmem_fd = create_mem_file(len + highmem);

	err = os_map_memory((void *) uml_reserved, physmem_fd, offset,
			    map_size, 1, 1, 1);
	if (err < 0) {
		printf("setup_physmem - mapping %ld bytes of memory at 0x%p "
		       "failed - errno = %d\n", map_size,
		       (void *) uml_reserved, err);
		exit(1);
	}

	/*
	 * Special kludge - This page will be mapped in to userspace processes
	 * from physmem_fd, so it needs to be written out there.
	 */
	os_seek_file(physmem_fd, __pa(__syscall_stub_start));
	os_write_file(physmem_fd, __syscall_stub_start, PAGE_SIZE);
	os_fsync_file(physmem_fd);

	bootmap_size = init_bootmem(pfn, pfn + delta);
	free_bootmem(__pa(reserve_end) + bootmap_size,
		     len - bootmap_size - reserve);
}

int phys_mapping(unsigned long phys, unsigned long long *offset_out)
{
	int fd = -1;

	if (phys < physmem_size) {
		fd = physmem_fd;
		*offset_out = phys;
	}
	else if (phys < __pa(end_iomem)) {
		struct iomem_region *region = iomem_regions;

		while (region != NULL) {
			if ((phys >= region->phys) &&
			    (phys < region->phys + region->size)) {
				fd = region->fd;
				*offset_out = phys - region->phys;
				break;
			}
			region = region->next;
		}
	}
	else if (phys < __pa(end_iomem) + highmem) {
		fd = physmem_fd;
		*offset_out = phys - iomem_size;
	}

	return fd;
}

static int __init uml_mem_setup(char *line, int *add)
{
	char *retptr;
	physmem_size = memparse(line,&retptr);
	return 0;
}
__uml_setup("mem=", uml_mem_setup,
"mem=<Amount of desired ram>\n"
"    This controls how much \"physical\" memory the kernel allocates\n"
"    for the system. The size is specified as a number followed by\n"
"    one of 'k', 'K', 'm', 'M', which have the obvious meanings.\n"
"    This is not related to the amount of memory in the host.  It can\n"
"    be more, and the excess, if it's ever used, will just be swapped out.\n"
"	Example: mem=64M\n\n"
);

extern int __init parse_iomem(char *str, int *add);

__uml_setup("iomem=", parse_iomem,
"iomem=<name>,<file>\n"
"    Configure <file> as an IO memory region named <name>.\n\n"
);

/*
 * This list is constructed in parse_iomem and addresses filled in in
 * setup_iomem, both of which run during early boot.  Afterwards, it's
 * unchanged.
 */
struct iomem_region *iomem_regions;

/* Initialized in parse_iomem and unchanged thereafter */
int iomem_size;

unsigned long find_iomem(char *driver, unsigned long *len_out)
{
	struct iomem_region *region = iomem_regions;

	while (region != NULL) {
		if (!strcmp(region->driver, driver)) {
			*len_out = region->size;
			return region->virt;
		}

		region = region->next;
	}

	return 0;
}
EXPORT_SYMBOL(find_iomem);

static int setup_iomem(void)
{
	struct iomem_region *region = iomem_regions;
	unsigned long iomem_start = high_physmem + PAGE_SIZE;
	int err;

	while (region != NULL) {
		err = os_map_memory((void *) iomem_start, region->fd, 0,
				    region->size, 1, 1, 0);
		if (err)
			printk(KERN_ERR "Mapping iomem region for driver '%s' "
			       "failed, errno = %d\n", region->driver, -err);
		else {
			region->virt = iomem_start;
			region->phys = __pa(region->virt);
		}

		iomem_start += region->size + PAGE_SIZE;
		region = region->next;
	}

	return 0;
}

__initcall(setup_iomem);
