#include "realmode.h"
#include "timer.h"
#include "latch.h"
#include "bios.h"

#define K_RDWR		0x60		/* keyboard data & cmds (read/write) */
#define K_STATUS	0x64		/* keyboard status */
#define K_CMD		0x64		/* keybd ctlr command (write-only) */

#define K_OBUF_FUL	0x01		/* output buffer full */
#define K_IBUF_FUL	0x02		/* input buffer full */

#define KC_CMD_WIN	0xd0		/* read  output port */
#define KC_CMD_WOUT	0xd1		/* write output port */
#define KB_SET_A20	0xdf		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */
#define KB_UNSET_A20	0xdd		/* enable A20,
					   enable output buffer full interrupt
					   enable data line
					   disable clock line */

enum { Disable_A20 = 0x2400, Enable_A20 = 0x2401, Query_A20_Status = 0x2402,
	Query_A20_Support = 0x2403 };

#define CF ( 1 << 0 )

#ifndef IBM_L40
static void empty_8042 ( void )
{
	unsigned long time;
	char st;

	time = currticks() + TICKS_PER_SEC;	/* max wait of 1 second */
	while ((((st = inb(K_CMD)) & K_OBUF_FUL) ||
	       (st & K_IBUF_FUL)) &&
	       currticks() < time)
		inb(K_RDWR);
}
#endif	/* IBM_L40 */

/*
 * Gate A20 for high memory
 *
 * Note that this function gets called as part of the return path from
 * librm's real_call, which is used to make the int15 call if librm is
 * being used.  To avoid an infinite recursion, we make gateA20_set
 * return immediately if it is already part of the call stack.
 */
void gateA20_set ( void ) {
	static char reentry_guard = 0;
	uint16_t flags, status;

	if ( reentry_guard )
		return;
	reentry_guard = 1;

	REAL_EXEC ( rm_enableA20,
		    "sti\n\t"
		    "stc\n\t"
		    "int $0x15\n\t"
		    "pushfw\n\t"
		    "popw %%bx\n\t"
		    "cli\n\t",
		    2,
		    OUT_CONSTRAINTS ( "=a" ( status ), "=b" ( flags ) ),
		    IN_CONSTRAINTS ( "a" ( Enable_A20 ) ),
		    CLOBBER ( "ecx", "edx", "ebp", "esi", "edi" ) );

	if ( ( flags & CF ) ||
	     ( ( status >> 8 ) & 0xff ) ) {
		/* INT 15 method failed, try alternatives */
#ifdef	IBM_L40
		outb(0x2, 0x92);
#else	/* IBM_L40 */
		empty_8042();
		outb(KC_CMD_WOUT, K_CMD);
		empty_8042();
		outb(KB_SET_A20, K_RDWR);
		empty_8042();
#endif	/* IBM_L40 */
	}
	
	reentry_guard = 0;
}

void gateA20_unset ( void ) {
	/* Not currently implemented */
}
