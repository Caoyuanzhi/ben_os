#include "uart.h"
#include "esr.h"
#include "irq.h"
#include "asm/base.h"
#include "mm.h"
#include "asm/barrier.h"

extern char _text_boot[], _etext_boot[];
extern char _text[], _etext[];
extern char _rodata[], _erodata[];
extern char _data[], _edata[];
extern char _bss[], _ebss[];
extern char _ttbr[];
extern char idmap_pg_dir[];

static void print_mem(void)
{
	printk("BenOS image layout:\n");
	printk("  .text.boot: 0x%08lx - 0x%08lx (%6ld B)\n",
			(unsigned long)_text_boot, (unsigned long)_etext_boot,
			(unsigned long)(_etext_boot - _text_boot));
	printk("       .text: 0x%08lx - 0x%08lx (%6ld B)\n",
			(unsigned long)_text, (unsigned long)_etext,
			(unsigned long)(_etext - _text));
	printk("     .rodata: 0x%08lx - 0x%08lx (%6ld B)\n",
			(unsigned long)_rodata, (unsigned long)_erodata,
			(unsigned long)(_erodata - _rodata));
	printk("       .data: 0x%08lx - 0x%08lx (%6ld B)\n",
			(unsigned long)_data, (unsigned long)_edata,
			(unsigned long)(_edata - _data));
	printk("        .bss: 0x%08lx - 0x%08lx (%6ld B)\n",
			(unsigned long)_bss, (unsigned long)_ebss,
			(unsigned long)(_ebss - _bss));
}

#define read_sysreg(reg) ({ \
		unsigned long _val; \
		asm volatile("mrs %0," #reg \
		: "=r"(_val)); \
		_val; \
})

#define write_sysreg(val, reg) ({ \
		unsigned long _val = (unsigned long)val; \
		asm volatile("msr " #reg ", %x0" \
		:: "rZ"(_val)); \
})

static void test_sysregs(void)
{
	unsigned long el;
	el = read_sysreg(CurrentEL);
	printk("-------Current_el = %d\n", el >> 2);
	write_sysreg(0x10000, sctlr_el3);
	printk("read vbar: 0x%x\n", read_sysreg(sctlr_el3));
}

static const char * const bad_mode_handler[] = {
	"Sync Abort",
	"IRQ",
	"FIQ",
	"SError"
};

static const char *esr_class_str[] = {
	[0 ... ESR_ELx_EC_MAX]		= "UNRECOGNIZED EC",
	[ESR_ELx_EC_UNKNOWN]		= "Unknown/Uncategorized",
	[ESR_ELx_EC_WFx]			= "WFI/WFE",
	[ESR_ELx_EC_CP15_32]		= "CP15 MCR/MRC",
	[ESR_ELx_EC_CP15_64]		= "CP15 MCRR/MRRC",
	[ESR_ELx_EC_CP14_MR]		= "CP14 MCR/MRC",
	[ESR_ELx_EC_CP14_LS]		= "CP14 LDC/STC",
	[ESR_ELx_EC_FP_ASIMD]		= "ASIMD",
	[ESR_ELx_EC_CP10_ID]		= "CP10 MRC/VMRS",
	[ESR_ELx_EC_CP14_64]		= "CP14 MCRR/MRRC",
	[ESR_ELx_EC_ILL]			= "PSTATE.IL",
	[ESR_ELx_EC_SVC32]			= "SVC (AArch32)",
	[ESR_ELx_EC_HVC32]			= "HVC (AArch32)",
	[ESR_ELx_EC_SMC32]			= "SMC (AArch32)",
	[ESR_ELx_EC_SVC64]			= "SVC (AArch64)",
	[ESR_ELx_EC_HVC64]			= "HVC (AArch64)",
	[ESR_ELx_EC_SMC64]			= "SMC (AArch64)",
	[ESR_ELx_EC_SYS64]			= "MSR/MRS (AArch64)",
	[ESR_ELx_EC_IMP_DEF]		= "EL3 IMP DEF",
	[ESR_ELx_EC_IABT_LOW]		= "IABT (lower EL)",
	[ESR_ELx_EC_IABT_CUR]		= "IABT (current EL)",
	[ESR_ELx_EC_PC_ALIGN]		= "PC Alignment",
	[ESR_ELx_EC_DABT_LOW]		= "DABT (lower EL)",
	[ESR_ELx_EC_DABT_CUR]		= "DABT (current EL)",
	[ESR_ELx_EC_SP_ALIGN]		= "SP Alignment",
	[ESR_ELx_EC_FP_EXC32]		= "FP (AArch32)",
	[ESR_ELx_EC_FP_EXC64]		= "FP (AArch64)",
	[ESR_ELx_EC_SERROR]			= "SError",
	[ESR_ELx_EC_BREAKPT_LOW]	= "Breakpoint (lower EL)",
	[ESR_ELx_EC_BREAKPT_CUR]	= "Breakpoint (current EL)",
	[ESR_ELx_EC_SOFTSTP_LOW]	= "Software Step (lower EL)",
	[ESR_ELx_EC_SOFTSTP_CUR]	= "Software Step (current EL)",
	[ESR_ELx_EC_WATCHPT_LOW]	= "Watchpoint (lower EL)",
	[ESR_ELx_EC_WATCHPT_CUR]	= "Watchpoint (current EL)",
	[ESR_ELx_EC_BKPT32]			= "BKPT (AArch32)",
	[ESR_ELx_EC_VECTOR32]		= "Vector catch (AArch32)",
	[ESR_ELx_EC_BRK64]			= "BRK (AArch64)",
};

static const char *esr_get_class_string(unsigned int esr)
{
	return esr_class_str[esr >> ESR_ELx_EC_SHIFT];
}

static const char *data_fault_code[] = {
	[0] 	= "Address size fault, level0",
	[1] 	= "Address size fault, level1",
	[2] 	= "Address size fault, level2",
	[3] 	= "Address size fault, level3",
	[4] 	= "Translation fault, level0",
	[5] 	= "Translation fault, level1",
	[6] 	= "Translation fault, level2",
	[7] 	= "Translation fault, level3",
	[9] 	= "Access flag fault, level1",
	[10] 	= "Access flag fault, level2",
	[11] 	= "Access flag fault, level3",
	[13] 	= "Permission fault, level1",
	[14] 	= "Permission fault, level2",
	[15] 	= "Permission fault, level3",
	[0x21]  = "Alignment fault",
	[0x35]  = "Unsupported Exclusive or Atomic access",
};

static const char *esr_get_dfsc_string(unsigned int esr)
{
	return data_fault_code[esr & 0x3f];
}

void parse_esr(unsigned int esr)
{
	unsigned int ec = esr >> ESR_ELx_EC_SHIFT;
	unsigned int il = (esr & ESR_ELx_IL);
	unsigned int dfsc = (esr & 0x3f);

	printk("ESR info:\n");
	printk("  ESR = 0x%08x\n", esr);
	printk("  EC = 0x%08x\n", ec);
	printk("  Exception class = %s, IL = %u bits\n", esr_class_str[ec], il ? 32 : 16);
	//Data_Abort:
	if (ec == ESR_ELx_EC_DABT_LOW || ec == ESR_ELx_EC_DABT_CUR) {
		printk("  Data abort:\n");
		if ((esr & ESR_ELx_ISV)) {
			printk("  Access size = %u byte(s)\n",
			 1U << ((esr & ESR_ELx_SAS) >> ESR_ELx_SAS_SHIFT));
			printk("  SSE = %lu, SRT = %lu\n",
			 (esr & ESR_ELx_SSE) >> ESR_ELx_SSE_SHIFT,
			 (esr & ESR_ELx_SRT_MASK) >> ESR_ELx_SRT_SHIFT);
			printk("  SF = %lu, AR = %lu\n",
			 (esr & ESR_ELx_SF) >> ESR_ELx_SF_SHIFT,
			 (esr & ESR_ELx_AR) >> ESR_ELx_AR_SHIFT);
		}
		printk("  SET = %lu, FnV = %lu\n",
			(esr >> ESR_ELx_SET_SHIFT) & 3,
			(esr >> ESR_ELx_FnV_SHIFT) & 1);
		printk("  EA = %lu, S1PTW = %lu\n",
			(esr >> ESR_ELx_EA_SHIFT) & 1,
			(esr >> ESR_ELx_S1PTW_SHIFT) & 1);
		printk("  CM = %lu, WnR = %lu\n",
		 (esr & ESR_ELx_CM) >> ESR_ELx_CM_SHIFT,
		 (esr & ESR_ELx_WNR) >> ESR_ELx_WNR_SHIFT);
		printk("  DFSC = %s\n", data_fault_code[dfsc]);
	}
}


void bad_mode(struct pt_regs *regs, int reason, unsigned int esr)
{
	printk("Bad mode for %s handler detected, far:0x%x esr:0x%x - %s\n",
			bad_mode_handler[reason], read_sysreg(far_el1),
			esr, esr_get_class_string(esr));
	parse_esr(esr);
}
extern void trigger_misalign(void);

static int test_access_map_address(void)
{
	unsigned long address = TOTAL_MEMORY - 4096;
	*(unsigned long *)address = 0x55;
	printk("%s access 0x%x done\n", __func__, address);
	return 0;
}



static void test_mmu(void)
{
	test_access_map_address();
	//test_access_unmap_address();
}



static void address_translate_test(void){
	//test address translate instruction
	asm volatile(
			"ldr x0, =0x80000\n"		\
			"at S1E1R, x0\n"	\
			:			
			:	
			: "x0"			
		    );

	unsigned long long par_el1_val;
	par_el1_val = read_sysreg(par_el1);
	unsigned long long f 	= par_el1_val & 1;
	unsigned long long sh 	= ( (par_el1_val & ((1UL << 9) -1) ) >>  6  ) << 6;
	unsigned long long ns 	= par_el1_val & (1UL << 9);
	unsigned long long pa 	= ( (par_el1_val & ((1UL <<48) -1) ) >> 12 ) << 12;
	unsigned long long attr = par_el1_val >> 56;
	printk("PA =%llx, F=%llx, SH =%llx, NS=%llx, ATTR=%llx \n", pa,f,sh,ns,attr);
}

void kernel_main(void){
	uart_init();
	init_printk_done();
	uart_send_string("Welcome BenOS!\r\n");
	
	print_mem();
	//FAULT
	//test_sysregs();
	//trigger_misalign();

	paging_init();
	printk("PAGE INIT DONE\n");

	//address_translate_test();
	//test_mmu();
	
	gic_init(0, GIC_V2_DISTRIBUTOR_BASE, GIC_V2_CPU_INTERFACE_BASE);
	timer_init();
	raw_local_irq_enable();


	while (1) {
		uart_send(uart_recv());
	}
}


