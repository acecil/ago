/* COMPILE: "cmake ." to generate Makefile, "make" to build ago library and ago_test test program. 
 */

/* a test of the lightweight thread implementation in ago */

/** Quick documentation:
 * Create ago object before doing anything,
 * ago::go() to start a lightweight thread,
 * and destruct ago object when you're finished.
 */
 
#include <iostream>
#include <mutex>
#include <functional>
#include "ago.h"

static std::mutex m;

static void worker(int *a)
{
	std::lock_guard<std::mutex> lock(m);
	std::cout << "Worker #" << *a << std::endl;
}

int main()
{
	ago r(4);
	int a[2048];
	
	for(int i = 0; i < 2048; ++i)
	{
		a[i] = i + 1;
		r.go(std::bind(&worker, a + i));
	}

	r.wait();

	std::cout << "after wait." << std::endl;

	return 0;
}
