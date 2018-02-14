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
#ifndef _UM_IO_H_
#define _UM_IO_H_

#include <linux/prints.h>

/* The next several definitions are from the asm(um)/io.h of kernel 3.2 -- start */
#define IO_SPACE_LIMIT 0xdeadbeef /* Sure hope nothing uses this */

static inline int inb(unsigned long i) { return(0); }
static inline void outb(char c, unsigned long i) { }

/*
 * Change virtual addresses to physical addresses and vv.
 * These are pretty trivial
 */
static inline unsigned long virt_to_phys(volatile void * address)
{
	return __pa((void *) address);
}

static inline void * phys_to_virt(unsigned long address)
{
	return __va(address);
}

/*
 * Convert a physical pointer to a virtual kernel pointer for /dev/mem
 * access
 */
#define xlate_dev_mem_ptr(p)	__va(p)

/*
 * Convert a virtual cached pointer to an uncached pointer
 */
#define xlate_dev_kmem_ptr(p)	p

/* from include/asm-generic/io.h */
static inline void __raw_writeb(u8 value, volatile void __iomem *addr)
{
	*(volatile u8 __force *)addr = value;
}
static inline void __raw_writew(u16 value, volatile void __iomem *addr)
{
	*(volatile u16 __force *)addr = value;
}
static inline void __raw_writel(u32 value, volatile void __iomem *addr)
{
	*(volatile u32 __force *)addr = value;
}
static inline void __raw_writeq(u64 value, volatile void __iomem *addr)
{
	*(volatile u64 __force *)addr = value;
}
#define writeb __raw_writeb
#define writew __raw_writew
#define writel __raw_writel
#define writeq __raw_writeq

/* from include/asm-generic/io.h */
static inline u8 __raw_readb(const volatile void __iomem *addr)
{
	return *(const volatile u8 __force *) addr;
}
static inline u16 __raw_readw(const volatile void __iomem *addr)
{
	return *(const volatile u16 __force *) addr;
}
static inline u32 __raw_readl(const volatile void __iomem *addr)
{
	return *(const volatile u32 __force *) addr;
}
static inline u64 __raw_readq(const volatile void __iomem *addr)
{
	return *(const volatile u64 __force *) addr;
}

#define readb __raw_readb
#define readw __raw_readw
#define readl __raw_readl
#define readq __raw_readq

#define iowrite32(v, addr)	writel((v), (addr))
#define ioread32(addr)		readl(addr)

/* from asm-generic/io.h -- start */
#ifndef writel_relaxed
#define writel_relaxed writel
#endif

static inline u8 inb_p(unsigned long addr)
{
        return inb(addr);
}

static inline void outb_p(u8 value, unsigned long addr)
{
        outb(value, addr);
}

static inline void outw(u16 value, unsigned long addr)
{
	BUG();
}

/* end */

void __iomem *ioremap(phys_addr_t offset, unsigned long size);

#define __ioremap(offset, size, flags)	ioremap(offset, size)

#define ioremap_nocache ioremap

#define ioremap_wc ioremap_nocache

void iounmap(void *addr);

static inline int io_remap_pfn_range_user(void *vma, unsigned long from,
		unsigned long pfn, unsigned long size, pgprot_t prot)
{
	return 0;
}

#define remap_pfn_range_user io_remap_pfn_range_user

static inline dma_addr_t
pci_map_page(void *hwdev, struct page *page,
	     unsigned long offset, size_t size, int direction)
{
	BUG();
}

static inline void
pci_unmap_page(void *hwdev, dma_addr_t dma_address,
	       size_t size, int direction)
{
}

static inline int
pci_dma_mapping_error(void *pdev, dma_addr_t dma_addr)
{

	return 0;
}

static inline void pci_iounmap(void *dev, void __iomem * addr)
{
}

static inline void *
pci_alloc_consistent(void *hwdev, size_t size,
		     dma_addr_t *dma_handle)
{
	return NULL;
}

#define	pci_free_consistent(_hwdev, _size, _vaddr, _dma_handle)

int pci_enable_msi_block(void *dev, unsigned int nvec);

#define pci_enable_msi(pdev)	pci_enable_msi_block(pdev, 1)

void pci_disable_msi(void *dev);

void __iomem *pci_map_rom(void *pdev, size_t *size);

void pci_unmap_rom(void *pdev, void __iomem *rom);

/* from linux/pci.h */
#ifdef CONFIG_PCI_BUS_ADDR_T_64BIT
typedef u64 pci_bus_addr_t;
#else
typedef u32 pci_bus_addr_t;
#endif

static inline pci_bus_addr_t pci_bus_address(void *pdev, int bar)
{

	BUG();
}

static inline int
pci_map_sg(void *hwdev, void *sg,
           int nents, int direction)
{
	BUG();
}

static inline void
pci_unmap_sg(void *hwdev, void *sg,
             int nents, int direction)
{
	BUG();
}

int pci_bus_alloc_resource(void *bus, void *res,
                 resource_size_t size, resource_size_t align,
                 resource_size_t min, unsigned long type_mask,
                 resource_size_t (*alignf)(void *,
                                           const void *,
                                           resource_size_t,
                                           resource_size_t),
                 void *alignf_data);

static inline resource_size_t pcibios_align_resource(void *data, const void *res,
                                 resource_size_t size,
                                 resource_size_t align)
{
	BUG();
}

static inline int pci_bus_read_config_word(void *bus, unsigned int devfn,
                              int where, u16 *val)
{
	BUG();
	return -EINVAL;
}

static inline int pci_bus_read_config_byte(void *bus, unsigned int devfn,
                              int where, u8 *val)
{
	BUG();
	return -EINVAL;
}

#define PCIBIOS_MIN_MEM  0

#define __io_virt(x) ((void __force *) (x))

#define memset_io(a, b, c)	memset(__io_virt(a), (b), (c))
#define memcpy_fromio(a, b, c)	memcpy((a), __io_virt(b), (c))
#define memcpy_toio(a, b, c)	memcpy(__io_virt(a), (b), (c))

/* from arch/x86/include/asm/paravirt.h */

#define rdmsrl(msr, val) BUG()

static inline int wbinvd_on_all_cpus(void)
{

	return 0;
}

int um_printf(const char *fmt);

#define rdtscll(val) BUG()

static __always_inline unsigned long long __native_read_tsc(void)
{
	BUG();

}

static inline void rdtsc_barrier(void)
{
	BUG();
}

static inline void __iomem *acpi_os_ioremap(u64 phys,
                                            u64 size)
{
	BUG();
}

#endif /* _UM_IO_H_ */
