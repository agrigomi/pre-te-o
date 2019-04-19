#ifndef __ATOM_H__
#define __ATOM_H__

#define ATOM_EXCHANGE_L(v1, v2) \
	__asm__ __volatile__ ("lock xchgl	%%eax, %[state]\n" \
				: [state] "=m" (v2), "=a"(v1) \
				:"a" (v1));



#endif

