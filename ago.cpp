/* lightweight goroutine-like threads for C++ */
/* by Andrew Gascoyne-Cecil based on C implementation by Alireza Nejati */

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
 * anything until ago::go() is called. We have a mutex protected queue that counts
 * how many *functions* there are waiting to be called, not how many
 * *threads* that are running idle. Each time ago::go() is called, the
 * oldest function in the queue is run in the next available thread. */

/* If all threads are currently busy, it waits until a thread becomes
 * free */
 
/* All accesses to the function queue are done in a mutex. Since they
 * are usually just reads or writes, they return very quickly */

/* We must provide a way for the main thread to wait until all functions
 * have been executed. This is ago::wait().
 * The mechanism behind it is condition variables.
 */

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
	
	/* queue of function pointers */
	std::queue<void (*)(void*)> func_list;
	std::queue<void*> arg_list;
	std::mutex func_mutex;

	/* these help with ago::wait */
	std::condition_variable idle_condition;
	std::condition_variable run_condition;
};

/**
 *  ago constructor
 *	max_conc: number of concurrent threads to run.
 */
ago::ago(int max_conc)
	: impl(new ago_impl)
{
	impl->ago_quit = false;
	

	/* Create required number of idle threads and add to thread list. */
	for(int i = 0; i < max_conc; ++i)
	{
		impl->thread_list.push_back(new std::thread(&ago::static_idle, this));
	}
}

/* waits until all threads are idle */
void ago::wait()
{
	/* Atomically wait until the function list is empty. */
	std::unique_lock<std::mutex> lock(impl->func_mutex);
	impl->idle_condition.wait(lock, [&]{ return impl->func_list.empty(); });
}

/** Destructor. Closes up all running threads.
 * If was in the middle of running functions, wait till they end.
 * Can restart again by creating a new ago object.
 */
ago::~ago()
{	
	/* tell all threads to quit */
	impl->ago_quit = true;

	/* Notify all threads to stop waiting. */
	impl->run_condition.notify_all();

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
 * */
void ago::go(void (*func)(void *), void *arg)
{		
	/* add function to stack */
	{
		std::lock_guard<std::mutex> lock(impl->func_mutex);
		impl->func_list.push(func);
		impl->arg_list.push(arg);
	}

	/* Tell once thread to wake up and execute the function. */
	impl->run_condition.notify_one();
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

		bool func_list_empty = false;
		{
			/* Atomically wait until a function is added to the list, or
			 * the quit variable is set.
			 */
			std::unique_lock<std::mutex> lock(impl->func_mutex);
			impl->run_condition.wait(lock, 
				[&]{ return (!impl->func_list.empty() || impl->ago_quit); });

			/* are we running functions or quitting? */
			if(impl->ago_quit) return;
		
			/* we have been assigned. Get the details of the function. */
			/* this must be done in a mutex to make sure two threads don't
			 * start to run the same function, if alib_go is called rapidly
			 * in succession */
			func = impl->func_list.front();
			impl->func_list.pop();
			arg  = impl->arg_list.front();
			impl->arg_list.pop();

			/* Capture if function list is empty with mutex locked.
			 * It doesn't matter if a function is added between this check
			 * and the notification.
			 */
			if(impl->func_list.empty())
			{
				func_list_empty = true;
			}
		}
		
		/* now run the function */
		func(arg);
		
		/* Signal if the function list is now empty number is zero */
		if(func_list_empty)
		{
			impl->idle_condition.notify_all();
		}
	}
};
