/*
 * Multiprocessor bootstrap.
 * Searches physical memory for MP description structures.
 * http://developer.intel.com/design/pentium/datashts/24201606.pdf
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from xv6 from MIT and Plan 9 from Bell Labs.
 * Adapted for PIOS by Bryan Ford at Yale University.
 */

//#include <inc/types.h>
//#include <inc/string.h>
#include "assert.h"

#include "cpu.h"
#include "mp.h"

#include "lapic.h"
#include "ioapic.h"

#include "string.h"
#include "x86.h"

int ismp;
int ncpu;
uint8_t ioapicid;
volatile struct ioapic *ioapic;


static uint8_t
sum(uint8_t * addr, int len)
{
	int i, sum;

	sum = 0;
	for (i = 0; i < len; i++)
		sum += addr[i];
	return sum;
}

//Look for an MP structure in the len bytes at addr.
static struct mp *
mpsearch1(uint8_t * addr, int len)
{
	assert(16 == sizeof(struct mp));
	uint8_t *e, *p;
	e = addr + len;
	for (p = addr; p < e; p += sizeof(struct mp))
		if (memcmp(p, "_MP_", 4) == 0 && sum(p, sizeof(struct mp)) == 0)
			return (struct mp *) p;
	return 0;
}

// Search for the MP Floating Pointer Structure, which according to the
// spec is in one of the following three locations:
// 1) in the first KB of the EBDA;
// 2) in the last KB of system base memory;
// 3) in the BIOS ROM between 0xE0000 and 0xFFFFF.
static struct mp *
mpsearch(void)
{
	uint8_t          *bda;
	uint32_t            p;
	struct mp      *mp;

	bda = (uint8_t *) 0x400;
	if ((p = ((bda[0x0F] << 8) | bda[0x0E]) << 4)) {
		if ((mp = mpsearch1((uint8_t *) p, 1024)))
			return mp;
	} else {
		p = ((bda[0x14] << 8) | bda[0x13]) * 1024;
		if ((mp = mpsearch1((uint8_t *) p - 1024, 1024)))
			return mp;
	}
	return mpsearch1((uint8_t *) 0xF0000, 0x10000);
}

// Search for an MP configuration table.  For now,
// don 't accept the default configurations (physaddr == 0).
// Check for correct signature, calculate the checksum and,
// if correct, check the version.
// To do: check extended table checksum.
static struct mpconf *
mpconfig(struct mp **pmp) {
	struct mpconf  *conf;
	struct mp      *mp;

	if ((mp = mpsearch()) == 0 || mp->physaddr == 0)
		return 0;
	conf = (struct mpconf *) mp->physaddr;
	if (memcmp(conf, "PCMP", 4) != 0)
		return 0;
	if (conf->version != 1 && conf->version != 4)
		return 0;
	if (sum((uint8_t *) conf, conf->length) != 0)
		return 0;
       *pmp = mp;
	return conf;
}

void
mp_init(void)
{
	uint8_t          *p, *e;
	struct mp      *mp;
	struct mpconf  *conf;
	struct mpproc  *proc;
	struct mpioapic *mpio;

	if (!cpu_onboot())	// only do once, on the boot CPU
		return;

	if ((conf = mpconfig(&mp)) == 0)
		return; // Not a multiprocessor machine - just use boot CPU.

	ismp = 1;
	lapic = (uint32_t *) conf->lapicaddr;
	for (p = (uint8_t *) (conf + 1), e = (uint8_t *) conf + conf->length;
			p < e;) {
		switch (*p) {
		case MPPROC:
			{
				proc = (struct mpproc *) p;
				p += sizeof(struct mpproc);
				if (!(proc->flags & MPENAB))
					continue;	// processor disabled

				// Get a cpu struct and kernel stack for this CPU.
				debug("cpu id = 0x")(proc->apicid, 16)();
				cpu *c = (proc->flags & MPBOOT)
						? g_cpu_boot : cpu_alloc();
				c->id = proc->apicid;
				ncpu++;
			}
			continue;
		case MPIOAPIC:
			mpio = (struct mpioapic *) p;
			p += sizeof(struct mpioapic);
			ioapicid = mpio->apicno;
			ioapic = (struct ioapic *) mpio->addr;
			debug("APIC id=0x")(ioapicid, 16)(" at 0x")(mpio->addr, 16)();
			continue;
		case MPBUS:
		case MPIOINTR:
		case MPLINTR:
			p += 8;
			continue;
		default:
			debug("mpinit: unknown config type 0x")(*p, 16)();
			die();
		}
	}
	if (mp->imcrp) {
		// Bochs doesn 't support IMCR, so this doesn' t run on Bochs.
		// But it would on real hardware.
		outb(0x22, 0x70);		// Select IMCR
		outb(0x23, inb(0x23) | 1);	// Mask external interrupts.
	}
}

