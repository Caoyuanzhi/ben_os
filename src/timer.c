#include <asm/timer.h>
#include <asm/irq.h>
#include <io.h>
#include <asm/arm_local_reg.h>
#include <timer.h>

#define HZ 250
#define NSEC_PER_SEC    1000000000L

static unsigned int val = NSEC_PER_SEC / HZ;

static int generic_timer_init(void)
{
	asm volatile(
		"mov x0, #1\n"
		"msr cntp_ctl_el0, x0"
		:
		:
		: "memory");

	return 0;
}

static int generic_timer_reset(unsigned int val)
{
	asm volatile(
		"msr cntp_tval_el0, %x[timer_val]"
		:
		: [timer_val] "r" (val)
		: "memory");

	return 0;
}

static void enable_timer_interrupt(void)
{
	writel(CNT_PNS_IRQ, TIMER_CNTRL0);
}

void timer_init(void)
{
	generic_timer_init();
	printk("---------XXXX\n");
	generic_timer_reset(val);
	printk("---------XXXX\n");
	gicv2_unmask_irq(GENERIC_TIMER_IRQ);
	printk("---------XXXX\n");
	enable_timer_interrupt();
	printk("---------XXXX\n");
}

void handle_timer_irq(void)
{
	generic_timer_reset(val);
	printk("---------XXXX\n");

	printk("Core0 Timer interrupt received\r\n");
}
