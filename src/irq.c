#include <asm/irq.h>
#include <io.h>
#include <asm/arm_local_reg.h>

void irq_handle(void)
{	
#if 0
	unsigned int irq = readl(ARM_LOCAL_IRQ_SOURCE0);
	printk("-----irq: %lx\r\n", irq);
	switch (irq) {
	case (CNT_PNS_IRQ):
		handle_timer_irq();  //reset timer val
		break;
	default:
		printk("Unknown pending irq: %x\r\n", irq);
	}
#else
	gic_handle_irq();
#endif
}
