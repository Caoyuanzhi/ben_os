#include <type.h>
#include <mm.h>
#include <asm/barrier.h>
#include <sysregs.h>
#include <asm/base.h>
#include <string.h>

//#define DEBUG_MMU
//#define DEBUG_MMU_BLOCK_MAP

#define KB (1 << 10)
#define MB (1 << 20)

#define PAGE_SHIFT	 			12
#define TABLE_SHIFT 			9
#define SECTION_SHIFT			(PAGE_SHIFT + TABLE_SHIFT) //21
#define NO_BLOCK_MAPPINGS       BIT(0)

#define PAGE_MASK 				(~(PAGE_SIZE-1))
#define SECTION_SIZE			(1 << SECTION_SHIFT)	//2MB
#define SECTION_MASK			(~(SECTION_SIZE-1))
//
//#define DEVICE_BASE         0x0FE000000
//#define DEVICE_SIZE         0x002000000
//#define DEVICE_BASE_END     0x100000000

//
#define ID_AA64MMFR0_TGRAN4_SHIFT		28
#define ID_AA64MMFR0_TGRAN64_SHIFT		24
#define ID_AA64MMFR0_TGRAN16_SHIFT		20
#define ID_AA64MMFR0_BIGENDEL0_SHIFT	16
#define ID_AA64MMFR0_SNSMEM_SHIFT		12
#define ID_AA64MMFR0_BIGENDEL_SHIFT		8
#define ID_AA64MMFR0_ASID_SHIFT			4
#define ID_AA64MMFR0_PARANGE_SHIFT		0

#define ID_AA64MMFR0_TGRAN4_NI			0xf
#define ID_AA64MMFR0_TGRAN4_SUPPORTED	0x0
#define ID_AA64MMFR0_TGRAN64_NI			0xf
#define ID_AA64MMFR0_TGRAN64_SUPPORTED	0x0
#define ID_AA64MMFR0_TGRAN16_NI			0x0
#define ID_AA64MMFR0_TGRAN16_SUPPORTED	0x1
#define ID_AA64MMFR0_PARANGE_48			0x5
#define ID_AA64MMFR0_PARANGE_52			0x6



/**************************************************************/
/**************************************************************/
/*
 * Memory types available.
 */
#define MT_DEVICE_nGnRnE	0
#define MT_DEVICE_nGnRE		1
#define MT_DEVICE_GRE		2
#define MT_NORMAL_NC		3
#define MT_NORMAL		4
#define MT_NORMAL_WT		5

#define MAIR(attr, mt)	((attr) << ((mt) * 8))
/*
mair = MAIR(0x00, MT_DEVICE_nGnRnE) |
       MAIR(0x04, MT_DEVICE_nGnRE) |
       MAIR(0x0c, MT_DEVICE_GRE) |
       MAIR(0x44, MT_NORMAL_NC) |
       MAIR(0xff, MT_NORMAL) |
       MAIR(0xbb, MT_NORMAL_WT);
write_sysreg(mair, mair_el1);
*/
/* CONFIG_ARM64_VA_BITS = 48*/
#define CONFIG_ARM64_VA_BITS 48
#define VA_BITS	 (CONFIG_ARM64_VA_BITS)
#define LV0_SHIFT 	39
#define LV1_SHIFT 	30
#define LV2_SHIFT 	21
#define LV3_SHIFT 	12

#define PAGE_SIZE 	(1UL << PAGE_SHIFT)

#define LV0_ENTRY_VA_RANGE 	(1UL << LV0_SHIFT)   //512GB    
#define LV1_ENTRY_VA_RANGE 	(1UL << LV1_SHIFT)   //1GB
#define LV2_ENTRY_VA_RANGE 	(1UL << LV2_SHIFT)   //2MB
#define LV3_ENTRY_VA_RANGE 	(1UL << LV3_SHIFT)   //4KB

#define LV0_PAGE_MASK 	(~(LV0_ENTRY_VA_RANGE-1))
#define LV1_PAGE_MASK 	(~(LV1_ENTRY_VA_RANGE-1))
#define LV2_PAGE_MASK 	(~(LV2_ENTRY_VA_RANGE-1))
#define LV3_PAGE_MASK 	(~(LV3_ENTRY_VA_RANGE-1))

#define LV0_ENTRY_NUM 	(1UL << (VA_BITS - LV0_SHIFT))    //512
#define LV1_ENTRY_NUM 	(1UL << (LV0_SHIFT - LV1_SHIFT))
#define LV2_ENTRY_NUM 	(1UL << (LV1_SHIFT - LV2_SHIFT))
#define LV3_ENTRY_NUM 	(1UL << (LV2_SHIFT - LV3_SHIFT))

#define LV0_TYPE_INVALID (0<<0)
#define LV1_TYPE_INVALID (0<<0)
#define LV2_TYPE_INVALID (0<<0)
#define LV3_TYPE_INVALID (0<<0)

#define LV0_TYPE_TABLE  (3<<0)
#define LV1_TYPE_TABLE  (3<<0)
#define LV2_TYPE_TABLE  (3<<0)

#define LV1_TYPE_BLOCK  (3<<0)
#define LV2_TYPE_BLOCK  (3<<0)
#define LV3_TYPE_BLOCK  (1<<0)


#define PUD_TYPE_SECT		(1UL << 0)
#define PMD_TYPE_SECT		(1UL << 0)
/*
 * block /scetion
 */
#define PMD_SECT_VALID		(1UL << 0)
#define PMD_SECT_USER		(1UL << 6)		/* AP[1] */
#define PMD_SECT_RDONLY		(1UL << 7)		/* AP[2] */
#define PMD_SECT_S		(3UL << 8)
#define PMD_SECT_AF		(1UL << 10)
#define PMD_SECT_NG		(1UL << 11)
#define PMD_SECT_CONT		(1UL << 52)
#define PMD_SECT_PXN		(1UL << 53)
#define PMD_SECT_UXN		(1UL << 54)
#define PMD_ATTRINDX(t)		((t) << 2)
#define PMD_ATTRINDX_MASK	(7UL << 2)

/* table descriptor*/
//TODO:

/* lv3 block descriptor*/
#define LV3_TYPE_PAGE  (3UL << 0)
#define LV3_TABLE_BIT  (1UL << 1)
#define LV3_USER       (1UL << 6)       /* AP[1] */
#define LV3_RDONLY     (1UL << 7)       /* AP[2] */
#define LV3_SHARED     (3UL << 8)       /* SH[1:0], inner shareable */
#define LV3_AF	       (1UL<< 10)	/* Access Flag */
#define LV3_NG	       (1UL<< 11)	/* nG */
#define LV3_DBM	       (1UL << 51)	/* Dirty Bit Management */
#define LV3_CONT       (1UL << 52)	/* Contiguous range */
#define LV3_PXN	       (1UL << 53)	/* Privileged XN */
#define LV3_UXN	       (1UL << 54)	/* User XN */
#define LV3_HYP_XN     (1UL << 54)	/* HYP XN */
#define LV3_ADDR_LOW   (((1UL << (48 - LV3_SHIFT)) - 1) << LV3_SHIFT)
#define LV3_ADDR_MASK Lv3_ADDR_LOW


/*
 * AttrIndx[2:0] encoding (mapping attributes defined in the MAIR* registers).
 */
#define PTE_ATTRINDX(t)		((t) << 2)
#define PTE_ATTRINDX_MASK	(7 << 2)
/*
 * Level 3 descriptor (PTE).
 */

#define PTE_TYPE_FAULT (0UL << 0)
#define PTE_TYPE_PAGE  (3UL << 0)
#define PTE_TABLE_BIT  (1UL << 1)
#define PTE_USER       (1UL << 6) /* AP[1] */
#define PTE_RDONLY     (1UL << 7) /* AP[2] */
#define PTE_SHARED     (3UL << 8) /* SH[1:0], inner shareable */
#define PTE_AF	       (1UL << 10)	/* Access Flag */
#define PTE_NG	       (1UL << 11)	/* nG */
#define PTE_DBM	       (1UL << 51)	/* Dirty Bit Management */
#define PTE_CONT       (1UL << 52)	/* Contiguous range */
#define PTE_PXN	       (1UL << 53)	/* Privileged XN */
#define PTE_UXN	       (1UL << 54)	/* User XN */
#define PTE_HYP_XN     (1UL << 54)	/* HYP XN */

#define PTE_ADDR_LOW (((1UL << (48 - PAGE_SHIFT)) - 1) << PAGE_SHIFT)
#define PTE_ADDR_MASK PTE_ADDR_LOW

/* TCR flags */
#define TCR_T0SZ_OFFSET		0
#define TCR_T1SZ_OFFSET		16
#define TCR_T0SZ(x)		    ((UL(64) - (x)) << TCR_T0SZ_OFFSET)
#define TCR_T1SZ(x)		    ((UL(64) - (x)) << TCR_T1SZ_OFFSET)
#define TCR_TxSZ(x)		    (TCR_T0SZ(x) | TCR_T1SZ(x))
#define TCR_TxSZ_WIDTH		6
#define TCR_T0SZ_MASK		(((UL(1) << TCR_TxSZ_WIDTH) - 1) << TCR_T0SZ_OFFSET)

#define TCR_EPD0_SHIFT		7
#define TCR_EPD0_MASK		(UL(1) << TCR_EPD0_SHIFT)
#define TCR_IRGN0_SHIFT		8
#define TCR_IRGN0_MASK		(UL(3) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_NC		(UL(0) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WBWA		(UL(1) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WT		(UL(2) << TCR_IRGN0_SHIFT)
#define TCR_IRGN0_WBnWA		(UL(3) << TCR_IRGN0_SHIFT)

#define TCR_EPD1_SHIFT		23
#define TCR_EPD1_MASK		(UL(1) << TCR_EPD1_SHIFT)
#define TCR_IRGN1_SHIFT		24
#define TCR_IRGN1_MASK		(UL(3) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_NC		(UL(0) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WBWA		(UL(1) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WT		(UL(2) << TCR_IRGN1_SHIFT)
#define TCR_IRGN1_WBnWA		(UL(3) << TCR_IRGN1_SHIFT)

#define TCR_IRGN_NC		    (TCR_IRGN0_NC | TCR_IRGN1_NC)
#define TCR_IRGN_WBWA		(TCR_IRGN0_WBWA | TCR_IRGN1_WBWA)
#define TCR_IRGN_WT		    (TCR_IRGN0_WT | TCR_IRGN1_WT)
#define TCR_IRGN_WBnWA		(TCR_IRGN0_WBnWA | TCR_IRGN1_WBnWA)
#define TCR_IRGN_MASK		(TCR_IRGN0_MASK | TCR_IRGN1_MASK)


#define TCR_ORGN0_SHIFT		10
#define TCR_ORGN0_MASK		(UL(3) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_NC		(UL(0) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WBWA		(UL(1) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WT		(UL(2) << TCR_ORGN0_SHIFT)
#define TCR_ORGN0_WBnWA		(UL(3) << TCR_ORGN0_SHIFT)

#define TCR_ORGN1_SHIFT		26
#define TCR_ORGN1_MASK		(UL(3) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_NC		(UL(0) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WBWA		(UL(1) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WT		(UL(2) << TCR_ORGN1_SHIFT)
#define TCR_ORGN1_WBnWA		(UL(3) << TCR_ORGN1_SHIFT)

#define TCR_ORGN_NC		    (TCR_ORGN0_NC | TCR_ORGN1_NC)
#define TCR_ORGN_WBWA		(TCR_ORGN0_WBWA | TCR_ORGN1_WBWA)
#define TCR_ORGN_WT		    (TCR_ORGN0_WT | TCR_ORGN1_WT)
#define TCR_ORGN_WBnWA		(TCR_ORGN0_WBnWA | TCR_ORGN1_WBnWA)
#define TCR_ORGN_MASK		(TCR_ORGN0_MASK | TCR_ORGN1_MASK)

#define TCR_SH0_SHIFT		12
#define TCR_SH0_MASK		(UL(3) << TCR_SH0_SHIFT)
#define TCR_SH0_INNER		(UL(3) << TCR_SH0_SHIFT)

#define TCR_SH1_SHIFT		28
#define TCR_SH1_MASK		(UL(3) << TCR_SH1_SHIFT)
#define TCR_SH1_INNER		(UL(3) << TCR_SH1_SHIFT)
#define TCR_SHARED		    (TCR_SH0_INNER | TCR_SH1_INNER)

#define TCR_TG0_SHIFT		14
#define TCR_TG0_MASK		(UL(3) << TCR_TG0_SHIFT)
#define TCR_TG0_4K		    (UL(0) << TCR_TG0_SHIFT)
#define TCR_TG0_64K		    (UL(1) << TCR_TG0_SHIFT)
#define TCR_TG0_16K		    (UL(2) << TCR_TG0_SHIFT)

#define TCR_TG1_SHIFT		30
#define TCR_TG1_MASK		(UL(3) << TCR_TG1_SHIFT)
#define TCR_TG1_16K		    (UL(1) << TCR_TG1_SHIFT)
#define TCR_TG1_4K		    (UL(2) << TCR_TG1_SHIFT)
#define TCR_TG1_64K		    (UL(3) << TCR_TG1_SHIFT)

#define TCR_IPS_SHIFT		32
#define TCR_IPS_MASK		(UL(7) << TCR_IPS_SHIFT)
#define TCR_A1			    (UL(1) << 22)
#define TCR_ASID16		    (UL(1) << 36)
#define TCR_TBI0		    (UL(1) << 37)
#define TCR_TBI1		    (UL(1) << 38)
#define TCR_HA			    (UL(1) << 39)
#define TCR_HD			    (UL(1) << 40)
#define TCR_NFD1		    (UL(1) << 54)

#define TCR_TG_FLAGS    (TCR_TG0_4K | TCR_TG1_4K)
#define TCR_KASLR_FLAGS 0
#define TCR_KASAN_FLAGS 0
#define TCR_SMP_FLAGS   TCR_SHARED
#define TCR_CACHE_FLAGS (TCR_IRGN_WBWA | TCR_ORGN_WBWA)

/*
 * Software defined PTE bits definition.
 */
#define PTE_VALID		(1UL << 0)
#define PTE_WRITE		(PTE_DBM)		 /* same as DBM (51) */
#define PTE_DIRTY		(1UL << 55)
#define PTE_SPECIAL		(1UL << 56)
#define PTE_PROT_NONE	(1UL << 58) /* only when !PTE_VALID */

/*
*/
#define _PROT_DEFAULT	(PTE_TYPE_PAGE | PTE_AF | PTE_SHARED)
#define PROT_DEFAULT    (_PROT_DEFAULT)

#define PAGE_KERNEL_RO		((PROT_NORMAL & ~PTE_WRITE) | PTE_RDONLY)
#define PAGE_KERNEL_ROX		((PROT_NORMAL & ~(PTE_WRITE | PTE_PXN)) | PTE_RDONLY)
#define PAGE_KERNEL_EXEC	(PROT_NORMAL & ~PTE_PXN)

#define PROT_DEVICE_nGnRnE	(PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_DIRTY | PTE_WRITE | PTE_ATTRINDX(MT_DEVICE_nGnRnE))
#define PROT_DEVICE_nGnRE	(PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_DIRTY | PTE_WRITE | PTE_ATTRINDX(MT_DEVICE_nGnRE))
#define PROT_NORMAL_NC		(PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_DIRTY | PTE_WRITE | PTE_ATTRINDX(MT_NORMAL_NC))
#define PROT_NORMAL_WT		(PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_DIRTY | PTE_WRITE | PTE_ATTRINDX(MT_NORMAL_WT))
#define PROT_NORMAL         (PROT_DEFAULT | PTE_PXN | PTE_UXN | PTE_DIRTY | PTE_WRITE | PTE_ATTRINDX(MT_NORMAL))

#define PAGE_KERNEL PROT_NORMAL

/**************************************************************/


/**************************************************************/
typedef unsigned long long u64;
typedef u64 lv3_pgt_entry_t;
typedef u64 lv2_pgt_entry_t;
typedef u64 lv1_pgt_entry_t;
typedef u64 lv0_pgt_entry_t;

typedef struct{
    lv3_pgt_entry_t lv3_pte; 
}lv3_pte_t;
#define get_lv3_pte_val(x) ((x).lv3_pte)
#define __lv3_pte(y) ( (lv3_pte_t){ (y) } ) //pte

typedef struct{
    lv2_pgt_entry_t lv2_pte; 
}lv2_pte_t;
#define get_lv2_pte_val(x) ((x).lv2_pte)
#define __lv2_pte(y) ( (lv2_pte_t){ (y) } ) //pte

typedef struct{
    lv1_pgt_entry_t lv1_pte; 
}lv1_pte_t;
#define get_lv1_pte_val(x) ((x).lv1_pte)
#define __lv1_pte(y) ( (lv1_pte_t){ (y) } ) //pte

typedef struct{
    lv0_pgt_entry_t lv0_pte; 
}lv0_pte_t;
#define get_lv0_pte_val(x) ((x).lv0_pte)
#define __lv0_pte(y) ( (lv0_pte_t){ (y) } ) //pte

#define pfn_pte(pfn, prot)  (__lv3_pte(((pfn) << LV3_SHIFT) | (prot)))
#define pfn_pmd(pfn, prot)  (__lv2_pte(((pfn) << LV2_SHIFT) | (prot)))
static inline unsigned long mk_sect_prot(unsigned long prot)
{
	return prot & ~PTE_TABLE_BIT;
}
/*****************************************************/

#define lv0_entry_none(lv0_pte) (!get_lv0_pte_val(lv0_pte))
#define lv1_entry_none(lv1_pte) (!get_lv1_pte_val(lv1_pte))
#define lv2_entry_none(lv2_pte) (!get_lv2_pte_val(lv2_pte))
#define lv3_entry_none(lv3_pte) (!get_lv3_pte_val(lv3_pte))

static inline void set_lv0_pte(lv0_pte_t *lv0_p, lv0_pte_t descriptor)
{
    *lv0_p = descriptor;
    dsb(ishst);
}

static inline void set_lv1_pte(lv1_pte_t *lv1_p, lv1_pte_t descriptor)
{
    *lv1_p = descriptor;
    dsb(ishst);
}

static inline void set_lv2_pte(lv2_pte_t *lv2_p, lv2_pte_t descriptor)
{
    *lv2_p = descriptor;
    dsb(ishst);
}

static inline void set_lv3_pte(lv3_pte_t *lv3_p, lv3_pte_t descriptor)
{
    *lv3_p = descriptor;
    dsb(ishst);
}


void set_lv2_block(lv2_pte_t *pmdp, unsigned long phys, unsigned long prot){
	unsigned long sect_prot = PMD_TYPE_SECT | mk_sect_prot(prot);
	lv2_pte_t new_pmd = pfn_pmd(phys >> 21 , sect_prot);
	set_lv2_pte(pmdp, new_pmd);
}


/**************************************************************/
/**************************************************************/

extern char _ttbr[];                     //TTBR 
extern char _text_boot[], _etext_boot[];//BOOT
extern char _text[], _etext[];          //TEST
extern char idmap_pg_dir[];

static unsigned long early_pgtable_alloc(void)
{
	unsigned long pa;
	pa = get_free_page(); // define in page_alloc.c
	memset((void *)pa, 0, PAGE_SIZE);
	return pa;
}


static inline unsigned long lv0_table_pa_addr(lv0_pte_t lv0_d)
{
	return get_lv0_pte_val(lv0_d) & PTE_ADDR_MASK;
}
static inline unsigned long lv1_table_pa_addr(lv1_pte_t lv1_d)
{
	return get_lv1_pte_val(lv1_d) & PTE_ADDR_MASK;
}
static inline unsigned long lv2_table_pa_addr(lv2_pte_t lv2_d)
{
	return get_lv2_pte_val(lv2_d) & PTE_ADDR_MASK;
}

/* lv0_index */
#define lv0_index(addr) (((addr) >> LV0_SHIFT) & (LV0_ENTRY_NUM -1))
#define lv0_offset_pa(ttbr, addr) ((ttbr) + lv0_index(addr)*8)
/* lv1_index */
#define lv1_index(addr) (((addr) >> LV1_SHIFT) & (LV1_ENTRY_NUM - 1))
#define lv1_offset_pa(lv0_base, addr) (lv0_pte_t *)(	lv0_table_pa_addr(*lv0_base) + lv1_index(addr)*8)
/* lv2_index */
#define lv2_index(addr) (((addr) >> LV2_SHIFT) & (LV2_ENTRY_NUM - 1))
#define lv2_offset_pa(lv1_base, addr) (lv1_pte_t *)(	lv1_table_pa_addr(*lv1_base) + lv2_index(addr)*8)
/* lv1_index */
#define lv3_index(addr) (((addr) >> LV3_SHIFT) & (LV3_ENTRY_NUM - 1))
#define lv3_offset_pa(lv2_base, addr) (lv2_pte_t *)(	lv2_table_pa_addr(*lv2_base) + lv3_index(addr)*8)



static void alloc_lv3_table(
        lv2_pte_t *lv2_p, 
        unsigned long addr,
	unsigned long end, 
        unsigned long pa,
	unsigned long prot,
	unsigned long (*alloc_pgtable)(void),
	unsigned long flags)
{
	lv2_pte_t lv2_descriptor = *lv2_p;
	lv3_pte_t *lv3_p;
	
    if (lv2_entry_none(lv2_descriptor)) {
		unsigned long long lv3_pa;
		lv3_pa = alloc_pgtable();
		set_lv2_pte(lv2_p, __lv2_pte(lv3_pa | LV2_TYPE_TABLE));
		//printk("-----CREATE_LV2_DESCRIPTOR\n");
		lv2_descriptor = *lv2_p;
		#ifdef DEBUG_MMU
			printk("-LV2_table_address:%lx\n",lv2_p);
		//	printk("----[LV2_DESCRIPTOR:%lx]\n",lv2_descriptor);
		#endif
	}

	lv3_p = lv3_offset_pa(lv2_p, addr);
	do {
		#ifdef DEBUG_MMU
		//printk("--------TEST: pfn_pte %llx\n", pfn_pte(pa >> PAGE_SHIFT, prot));
		#endif
		set_lv3_pte(lv3_p, pfn_pte(pa >> PAGE_SHIFT, prot));
		#ifdef DEBUG_MMU
		    lv3_pte_t st_descriptor = *lv3_p;
		    unsigned long long descriptor = *(unsigned long long *) &(st_descriptor);
		    unsigned long long pa_val 	= ( (descriptor & ((1UL<<48)-1)) >> 12 ) << 12;
		    unsigned long long xn 	= ( descriptor & (1UL<<54))   >> 54;
		    unsigned long long pxn 	= ( descriptor & (1UL<<53))   >> 53;
		    unsigned long long cont 	= ( descriptor & (1UL<<52))   >> 52;
		    unsigned long long dbm 	= ( descriptor & (1UL<<51))   >> 51;
		    unsigned long long ng 	= ( descriptor & (1UL<<11))   >> 11;
		    unsigned long long af 	= ( descriptor & (1UL<<10))   >> 10;
		    unsigned long long sh 	= ( descriptor & (1UL<<10)-1) >> 8;
		    unsigned long long ap 	= ( descriptor & (1UL<<8 )-1) >> 6;
		    unsigned long long ns 	= ( descriptor & (1UL<<5 ))   >> 5;
		    unsigned long long attr_idx = ( descriptor & (1UL<<5)-1)  >> 2;
		    printk("[CURR_VA] : %lx [LV3_DESC] : %lx [PA] : %lx [XN] : %lx [PXN] : %lx [AF] : %lx [SH] : %lx [AP] : %lx [ATTR_IDX] : %lx \n",addr,descriptor, pa_val, xn, pxn, af, sh, ap, attr_idx);
		#endif
		pa += PAGE_SIZE;
	} while (lv3_p++, addr += PAGE_SIZE, addr != end);
}

static void alloc_lv2_table(
        lv1_pte_t *lv1_p, 
        unsigned long addr,
	unsigned long end, 
        unsigned long pa,
	unsigned long prot,
	unsigned long (*alloc_pgtable)(void),
	unsigned long flags)
{
	lv1_pte_t lv1_descriptor = *lv1_p;
	lv2_pte_t *lv2_p;
	unsigned long next;

	if (lv1_entry_none(lv1_descriptor)) {
		unsigned long lv2_pa;
		lv2_pa = alloc_pgtable();
		set_lv1_pte(lv1_p, __lv1_pte(lv2_pa | LV1_TYPE_TABLE));
		//printk("----CREATE_LV1_DESCRIPTOR\n");
        	lv1_descriptor = *lv1_p;
		#ifdef DEBUG_MMU
			printk("----[LV1_DESCRIPTOR:%lx]\n",lv1_descriptor);
		#endif
	}

	lv2_p = lv2_offset_pa(lv1_p, addr);
	do {
        	unsigned long long boundary = (addr + LV2_ENTRY_VA_RANGE) & LV2_PAGE_MASK;
        	next = (boundary  < end ) ? boundary : end ;
		#ifdef DEBUG_MMU_BLOCK_MAP
			printk("--------lv2------addr %lx | end  %lx | boundary %lx \n",addr,end,boundary);
		#endif
		//BLOCK_DESCRIPTOR
		if( ((addr | next | pa) & ~SECTION_MASK) == 0 &&(flags & NO_BLOCK_MAPPINGS) == 0 ){
			#ifdef DEBUG_MMU_BLOCK_MAP
				printk("-----SET_BLOCK : addr %lx | next  %lx | pa %lx \n",addr,next,pa);
			#endif
			set_lv2_block(lv2_p, pa, prot);
			#ifdef DEBUG_MMU
		    	lv2_pte_t st_descriptor = *lv2_p;
		    	unsigned long long descriptor   = *(unsigned long long *) &(st_descriptor);
		    	unsigned long long pa_val 	= ( (descriptor & ((1UL<<48)-1)) >> 21 ) << 21;
		    	unsigned long long xn 		= ( descriptor & (1UL<<54))   >> 54;
		    	unsigned long long pxn 		= ( descriptor & (1UL<<53))   >> 53;
		    	unsigned long long cont 	= ( descriptor & (1UL<<52))   >> 52;
		    	unsigned long long dbm 		= ( descriptor & (1UL<<51))   >> 51;
		    	unsigned long long ng 		= ( descriptor & (1UL<<11))   >> 11;
		    	unsigned long long af 		= ( descriptor & (1UL<<10))   >> 10;
		    	unsigned long long sh 		= ( descriptor & (1UL<<10)-1) >> 8;
		    	unsigned long long ap 		= ( descriptor & (1UL<<8 )-1) >> 6;
		    	unsigned long long ns 		= ( descriptor & (1UL<<5 ))   >> 5;
		    	unsigned long long attr_idx 	= ( descriptor & (1UL<<5)-1)  >> 2;
		    	printk("[CURR_VA] : %lx [LV2_BLOCK_DESC] : %lx [PA] : %lx [XN] : %lx [PXN] : %lx [AF] : %lx [SH] : %lx [AP] : %lx [ATTR_IDX] : %lx \n",addr,descriptor, pa_val, xn, pxn, af, sh, ap, attr_idx);
		#endif
		}
		else{	//LV3_PTE
			//printk("MARK2----------\n");
			alloc_lv3_table(lv2_p, addr, next, pa, prot, alloc_pgtable, flags);
		}
		pa += next - addr;
	} while (lv2_p++, addr = next, addr != end);
}

static void alloc_lv1_table(
        lv0_pte_t *lv0_p, 
        unsigned long addr,
	unsigned long end, 
        unsigned long pa,
	unsigned long prot,
	unsigned long (*alloc_pgtable)(void),
	unsigned long flags)
{
	lv0_pte_t lv0_desriptor = *lv0_p;
	lv1_pte_t *lv1_p;
	unsigned long next;

	if (lv0_entry_none(lv0_desriptor)) {
		unsigned long lv1_pa;
		lv1_pa = alloc_pgtable();
		set_lv0_pte(lv0_p, __lv0_pte(lv1_pa | LV0_TYPE_TABLE)); //set lv0_descriptor
		//printk("---CREATE_LV0_DESCRIPTOR\n");
		lv0_desriptor = *lv0_p;
		//#ifdef DEBUG_MMU
			printk("-[LV0_table_address:%lx]\n",lv0_p);
			printk("-[LV0_DESCRIPTOR:%lx]\n",lv0_desriptor);
		//#endif
	}

	lv1_p = lv1_offset_pa(lv0_p, addr);
	//#ifdef DEBUG_MMU
		printk("-[LV1_table_address:%lx]\n",lv1_p);
	//#endif
	do {        

        	unsigned long long boundary = (addr + LV1_ENTRY_VA_RANGE ) & LV1_PAGE_MASK;
        	next = (boundary  < end ) ? boundary : end ;
		#ifdef DEBUG_MMU_BLOCK_MAP
			printk("--------lv1------addr %lx | end  %lx | boundary %lx \n",addr,end,boundary);
		#endif
		alloc_lv2_table(lv1_p, addr, next, pa, prot, alloc_pgtable, flags);
		pa += next - addr;

	} while (lv1_p++, addr = next, addr != end);
}


static void create_lv0_mapping(
        lv0_pte_t *ttbr, //ttbr
        	unsigned long pa_start,     
		unsigned long va_start, 
        	unsigned long size,
		unsigned long prot,
		unsigned long (*alloc_pgtable)(void),
		unsigned long flags)
{
	lv0_pte_t *lv0_p = lv0_offset_pa(ttbr, va_start);
	#ifdef DEBUG_MMU
		printk("-[LV0_table_address:%lx]\n",lv0_p);	
	#endif
    	unsigned long addr, end, next;
    	unsigned long pa;
	pa = pa_start & PAGE_MASK;
	addr = va_start & PAGE_MASK;
	end  = PAGE_ALIGN(va_start + size);

	do {
        	unsigned long boundary = (addr + LV0_ENTRY_VA_RANGE) & LV0_PAGE_MASK;
        	next = (boundary  < end ) ? boundary : end ;
		alloc_lv1_table(lv0_p, addr, next, pa, prot, alloc_pgtable, flags);
		pa += next - addr;
	} while (lv0_p++, addr = next, addr != end);
}


static void create_direct_mapping(){
    unsigned long va_start;
    unsigned long pa_start;
    unsigned long va_end;

    /*mapping the text*/
    va_start = (unsigned long) _text_boot;
    va_end   = (unsigned long) _etext;
    pa_start = va_start;
    
    create_lv0_mapping(
        (lv0_pte_t *) idmap_pg_dir,
        va_start,
        pa_start,
        va_end - va_start,
        PAGE_KERNEL_ROX,     
        early_pgtable_alloc,
        0
    );
	printk("--------------------------------------MAPPING_TEXT_DONE\n");
	/*mapping the memory*/
   va_start = PAGE_ALIGN((unsigned long) _etext);
   va_end   = TOTAL_MEMORY;
   pa_start = va_start;

   //printk("-------TEST: MEMORY_VA_START=%llx, VA_END =%llx\n",va_start, va_end);
   
   create_lv0_mapping(
       (lv0_pte_t *) idmap_pg_dir,
       pa_start,
       va_start,
       va_end - va_start,
       PAGE_KERNEL,  
       early_pgtable_alloc,
       1
   );
	printk("--------------------------------------MAPPING_MEMORY_DONE\n");
}

static void create_mmio_mapping(void){
    create_lv0_mapping(
        (lv0_pte_t *) idmap_pg_dir, 
        PBASE, 
        PBASE,
	DEVICE_SIZE,
        PROT_DEVICE_nGnRnE,
	early_pgtable_alloc,
	0
     );
	printk("--------------------------------------MAPPING_MMIO_DONE\n");
}

static void cpu_init(void){
    unsigned long mair = 0;
    unsigned long tcr = 0;
    unsigned long id_aa64mmfr0;
    unsigned long pa_range;
  
    asm("tlbi vmalle1");
    dsb(nsh);
  
    //write_sysreg(3UL << 20, cpacr_el1);
    //write_sysreg(1 << 12, mdscr_el1);          
	mair = MAIR(0x00UL, MT_DEVICE_nGnRnE) |
           MAIR(0x04UL, MT_DEVICE_nGnRE) |
           MAIR(0x0cUL, MT_DEVICE_GRE) |
           MAIR(0x44UL, MT_NORMAL_NC) |
           MAIR(0xffUL, MT_NORMAL) |
           MAIR(0xbbUL, MT_NORMAL_WT);
	write_sysreg(mair, mair_el1);

	unsigned long long mair_val;
	mair_val = read_sysreg(mair_el1);
	for(int i = 0; i<8; i++){
	    printk("--------------TEST : MAIR_EL1_[%d]=%llx\n",i,mair_val & ((1UL <<8 ) -1) );
	    mair_val = mair_val >> 8;
	}
	//memory model feature register 0
    	id_aa64mmfr0 = read_sysreg(ID_AA64MMFR0_EL1); 
    	pa_range = id_aa64mmfr0 & 0xf; 
            
    	tcr =  TCR_T0SZ(VA_BITS) | TCR_T1SZ(VA_BITS) | TCR_TG0_4K |TCR_TG1_4K;
    	tcr |= pa_range << TCR_IPS_SHIFT;    
	write_sysreg(tcr, tcr_el1);
 	
	//unsigned long tcr_val;
 	//tcr_val = read_sysreg(tcr_el1);
   	//printk("--------------TEST: TCR %lx\n",tcr_val);
	printk("cpu_init_done\n");
}

static int enable_mmu(void){
   	 unsigned long id_aa64mmfr0;
   	 int tgran4;
 	id_aa64mmfr0 = read_sysreg(ID_AA64MMFR0_EL1);
 	tgran4 = (id_aa64mmfr0 >> ID_AA64MMFR0_TGRAN4_SHIFT) & 0xf;
 	if (tgran4 != ID_AA64MMFR0_TGRAN4_SUPPORTED)
     	return -1; 

   	 write_sysreg(idmap_pg_dir, ttbr0_el1);
   	 isb();
	unsigned long ttbr;
	ttbr = read_sysreg(ttbr0_el1);
	printk("--------------TEST: TTBR %llx\n",ttbr);
	unsigned long _val;
	asm volatile(
			"adr %0,#4\n"
			:"+r"(_val)::
			);
	printk("--------------TEST: PC+4 %llx\n",_val);
	unsigned long sctlr;
	sctlr = read_sysreg(sctlr_el1);
	printk("--------------TEST: SCTLR %llx\n",sctlr);
	asm volatile(
			"ldr x0, =0x82628\n" 	\
			"at S1E1R, x0\n"	\
			:::"x0"
		);
	unsigned long par_val;
	par_val = read_sysreg(par_el1);
	printk("--------------TEST: PAR %llx\n",par_val);

	//asm volatile("svc #0");
 	write_sysreg( (sctlr | SCTLR_ELx_M), sctlr_el1);
   	 isb();
	sctlr = read_sysreg(sctlr_el1);
	printk("--------------TEST: SCTLR %lx\n",sctlr);

 
    	asm("ic iallu");
    	dsb(nsh);
    	isb();
	
	return 0;
}



void paging_init(void){
	printk("-----NO_BLOCK_MAPPINGS=%lx\n",NO_BLOCK_MAPPINGS);

	printk("PAGE_KERNEL_ROX: %lx\n",PAGE_KERNEL_ROX);
	/*clear the 4KB memory*/
	memset(idmap_pg_dir, 0, PAGE_SIZE);
	//
	create_direct_mapping();
	//
	create_mmio_mapping();
	//
	cpu_init();
	//
	enable_mmu();
	//
	printk("mmu_enable_done \n");
}










