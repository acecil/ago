/* COMPILE: -Wall alib/go.c -lpthread
 */

/* a test of the lightweight thread implementation in alib_go */

/** Quick documentation:
 * Call alib_thread_init() before doing anything,
 * alib_go() to start a lightweight thread,
 * and alib_thread_end() when you're finished.
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
