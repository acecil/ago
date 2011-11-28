/* COMPILE: "cmake ." to generate Makefile, "make" to build ago library and ago_test test program. 
 */

/* a test of the lightweight thread implementation in ago */

/** Quick documentation:
 * Create ago object before doing anything,
 * ago::go() to start a lightweight thread,
 * and destruct ago object when you're finished.
 */
 
#include <stdio.h>
#include "ago.h"

void worker(void *a)
{
	printf("Worker #%d\n", *((int *)a));
}

int main()
{
	ago r(4);
	int a[256];
	
	for(int i = 0; i < 256; ++i)
	{
		a[i]=i+1;
		r.go(&worker,a+i);
	}

	r.wait();

	return 0;
}
