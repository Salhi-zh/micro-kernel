#ifndef IRQ_H
#define IRQ_H

#include <kernel.h>
#include <isr.h>   /* for registers_t */

/* Function type for IRQ handlers */
typedef void (*irq_handler_t)(registers_t*);

/* Public functions */
void irq_init(void);
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);
void irq_unmask(int irq);
/* IRQ assembly stubs (0-15) */
extern void irq0(void);
extern void irq1(void);
extern void irq2(void);
extern void irq3(void);
extern void irq4(void);
extern void irq5(void);
extern void irq6(void);
extern void irq7(void);
extern void irq8(void);
extern void irq9(void);
extern void irq10(void);
extern void irq11(void);
extern void irq12(void);
extern void irq13(void);
extern void irq14(void);
extern void irq15(void);

#endif
