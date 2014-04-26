/*
 * oneway_channel.h
 *
 *  Created on: Apr 13, 2014
 *      Author: skim
 */

#ifndef ONEWAY_CHANNEL_H_
#define ONEWAY_CHANNEL_H_

#include <mutex>
#include <condition_variable>
#include <vector>
#include <list>
#include <atomic>

#include <csp/spaghetti.h>

namespace csp{

// Typical programming channel
// Blocks reading till stuff is written
// Never blocks writes!
// Cache isn't implemented here because it would require the holder
//  of input and output channels to know the cache sizes of each other
template<typename T>
class message_stream
{
private:
	std::mutex wait_lock_m;
	// Locked when reading/writing
	std::mutex flush;
	std::unique_lock<std::mutex> wait_lock_ml;
	std::list<std::array<T, CSP_CACHE_DEFAULT>> list;
	int readhead, writehead;
	int listsiz;
public:
	T last_read;
	// Wait on this for input
	std::condition_variable wait_lock;

	std::atomic_bool finished;
	std::mutex waiting;

	// Set true to always lock on read/write
	bool always_lock;

	message_stream()
	{
		always_lock = false;
		readhead = listsiz = writehead = 0;
		finished = false;
		wait_lock_ml = std::unique_lock<std::mutex>(wait_lock_m);
	}

	// Returns true if we need another go-around
	bool do_read(bool dolock)
	{
		if (readhead == CSP_CACHE_DEFAULT)
		{
			if (dolock)
				lock_this();

			readhead = 0;
			list.pop_front();
			listsiz--;

			if (dolock)
				unlock_this();
			return true;
		}
		last_read = list.front()[readhead++];
		return false;
	}

	bool safe_read()
	{
		retry_wlock:
		lock_this();
		retry:

		if (listsiz == 0)
			if (finished)
				return false;
			else
			{
				unlock_this();
				wait_write();
				goto retry_wlock;
			}
		// If we are down to one cache, think carefully about read/write heads
		else if (listsiz == 1)
		{
			if (readhead >= writehead)
				if (finished)
					return false;
				else
				{
					unlock_this();
					wait_write();
					goto retry_wlock;
				}
			else
				if (do_read(false))
					goto retry;
		}
		// We have more than one cache remaining in list
		else
			if (do_read(false))
				goto retry;
		unlock_this();

		return true;
	}

	bool read(T& t)
	{
		// Avoid locking, if list has plenty of data,
		// the data has been written back and is safe to read
		// We will lock if it is possible writes are messing with head of list
		retry:
		if (!always_lock && listsiz > 2)
		{
			if (do_read(true))
				goto retry;
		}
		else if (!safe_read())
			return false;
		t = last_read;
		return true;
	}

	void do_write(const T& t, bool dolock)
	{
		if (writehead == CSP_CACHE_DEFAULT)
		{
			if (dolock)
				lock_this();
			std::array<T, CSP_CACHE_DEFAULT> adder;
			list.push_back(adder);
			listsiz++;
			writehead = 0;

			if (dolock)
				unlock_this();

			// Threads will only be waiting on this read
			//   if the size is 0, 1, or 2
			if (listsiz < 3)
				wait_lock.notify_all();
		}
		list.back()[writehead++] = t;
	}
	void safe_write(const T& t)
	{
		lock_this();
		// Keep track if we are in a thread-safe mode
		if (listsiz == 0)
		{
			list.resize(1);
			listsiz++;
			writehead = 0;
		}
		do_write(t, false);
		unlock_this();
	}
	void write(const T& t)
	{
		// We will lock if writing will mess with the head of the list
		if (!always_lock && listsiz > 2)
			do_write(t, true);
		else
			safe_write(t);
	}

	void done()
	{
		finished = true;
		waiting.try_lock();
		wait_lock.notify_all();
	}

	void lock_this()
	{
		flush.lock();
	}
	void unlock_this()
	{
		flush.unlock();
	}

	void unlock_write()
	{
		flush.unlock();
		wait_lock.notify_all();
	}
	void wait_write()
	{
		if (!waiting.try_lock())
			return;
		wait_lock.wait(wait_lock_ml);
		waiting.unlock();
	}
};

}

#endif /* ONEWAY_CHANNEL_H_ */
