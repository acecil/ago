/* lightweight goroutine-like threads for C */
/* by Alireza Nejati */

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA. 
 */

/**The way this is done is we have several idle threads that don't do
 * anything until alib_go() is called. We have a semaphore that counts
 * how many *functions* there are waiting to be called, not how many
 * *threads* that are running idle. Each time alib_go() is called, the
 * semaphore count is decreased which causes one thread to run, pop the
 * next function off the function stack, and run it. */

/* If all threads are currently busy, it waits until a thread becomes
 * free */
 
/* All accesses to the function stack are done in a mutex. Since they
 * are usually just reads or writes, they return very quickly */

/* We must provide a way for the main thread to wait until all functions
 * have been executed. This is alib_thread_wait().
 * The mechanism behind it is condition variables.
 * Now, the proper implementation of sem_getvalue() would return the
 * number of threads waiting on the semaphore. Unfortunately, in linux
 * just 0 is returned, necessitating that we keep our own variable.
 * This is nrunning. */
/* We (atomically) increment nrunning whenever a new function is
 * dispatched, and (again, atomically) whenever the function completes.
 * Thus, when it becomes 0 we are guaranteed that all threads are idle
 * and no functions are in the pipeline, waiting to be executed. */

#include <thread>
#include <list>
#include <queue>
#include <mutex>
#include <condition_variable>

#include "ago.h"

struct ago::ago_impl
{
	/* the quit message */
	bool ago_quit;

	/* list of thread pointers */
	std::list<std::thread*> thread_list;
	
	/* stack of function pointers.
	 * By having one for each thread, there is no danger */
	std::queue<void (*)(void*)> func_list;
	std::queue<void*> arg_list;
	std::mutex func_mutex;

	/* these help with alib_thread_wait */
	std::mutex idle_mutex;
	std::condition_variable idle_condition;

	std::mutex run_mutex;
	std::condition_variable run_condition;
};

/**
 *  Initializes an alib_thread session. Need to call this before calling
 *  alib_go().
	max_conc: number of concurrent threads to run.
	Return value: number of actual threads created.
	Negative if error.
	Before returning, waits until all threads have been created.
	(If that isn't done, we risk posting to the semaphore before anyone
	 waits for it).
*/
ago::ago(int max_conc)
	: impl(new ago_impl)
{
	impl->ago_quit = false;
		
	for(int i = 0; i < max_conc; ++i)
	{
		impl->thread_list.push_back(new std::thread(&ago::static_idle, this));
	}
}

/* waits until all threads are idle */
void ago::wait()
{
	impl->idle_mutex.lock();
	impl->idle_condition.wait(std::unique_lock<std::mutex>(impl->idle_mutex));
	impl->idle_mutex.unlock();
}

/** Closes up all running threads.
 * If was in the middle of running functions, wait till they end.
 * Subsequent calls to alib_go will fail.
 * Can restart again with alib_thread_init()
 * Returns 0 if successfull, otherwise error code.
 */
ago::~ago()
{	
	/* tell all threads to quit */
	impl->ago_quit = true;

	impl->run_mutex.lock();
	impl->run_condition.notify_all();
	impl->run_mutex.unlock();

	/* wait for all threads to quit */
	auto t = begin(impl->thread_list);
	for(; t != end(impl->thread_list); ++t)
	{
		(*t)->join();
	}

	/* delete all threads */
	while( !impl->thread_list.empty() )
	{
		auto thread = impl->thread_list.back();
		delete thread;
		impl->thread_list.pop_back();
	}
}

/** Execute function func in parallel.
 * Returns 0 if successfull, error otherwise
 * (maybe due to exceeding maximum thread count.)
 * */
void ago::go(void (*func)(void *), void *arg)
{		
	/* add function to stack */
	impl->func_mutex.lock();
	impl->func_list.push(func);
	impl->arg_list.push(arg);
	impl->func_mutex.unlock();

	impl->run_mutex.lock();
	impl->run_condition.notify_one();
	impl->run_mutex.unlock();
}

/** Idling function.
	* Designed to block (not do anything) until a function has been
	* assigned to it.
	* See description at top of file. */
void ago::static_idle(ago *obj)
{
	obj->idle();
}
void ago::idle()
{
	void (*func)(void*);
	void *arg;
	
	/* idling loop */
	while(1){

		impl->run_mutex.lock();
		impl->run_condition.wait(std::unique_lock<std::mutex>(impl->run_mutex));
		impl->run_mutex.unlock();

		/* are we running functions or quitting? */
		if(impl->ago_quit) return;
		
		/* we have been assigned. Get the details of the function. */
		/* this must be done in a mutex to make sure two threads don't
			* start to run the same function, if alib_go is called rapidly
			* in succession */
		impl->func_mutex.lock();
		func = impl->func_list.front();
		impl->func_list.pop();
		arg  = impl->arg_list.front();
		impl->arg_list.pop();
		bool funcListEmpty = impl->func_list.empty();
		impl->func_mutex.unlock();
		
		/* now run the function */
		func(arg);
		
		/* decrement the number of running functions, and signal if the
		 * number is zero */
		impl->idle_mutex.lock();
		if(funcListEmpty)
		{
			impl->idle_condition.notify_all();
		}
		impl->idle_mutex.unlock();
	}
};
