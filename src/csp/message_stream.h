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

namespace CSP{

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
	std::atomic_int listsiz;
public:
	T last_read;
	// Wait on this for input
	std::condition_variable wait_lock;

	std::atomic_bool finished;

	message_stream()
	{
		readhead = listsiz = writehead = 0;
		finished = false;
		wait_lock_ml = std::unique_lock<std::mutex>(wait_lock_m);
	}

	// Returns true if we need another go-around
	bool do_read(bool islocking)
	{
		if (readhead == CSP_CACHE_DEFAULT)
		{
			if (!islocking)
				lockthis();
			readhead = 0;
			list.pop_front();
			listsiz--;
			if (!islocking)
				careful_unlock();
			return true;
		}
		last_read = list.front()[readhead++];
		return false;
	}
	bool safe_read(const bool lock)
	{
		retry:
		if (lock) lockthis();

		if (listsiz == 0)
			if (finished)
				return false;
			else
			{
				if (lock) careful_unlock();
				wait_write();
				goto retry;
			}
		// If we are down to one cache, think carefully about read/write heads
		else if (listsiz == 1)
		{
			if (readhead >= writehead)
				if (finished)
					return false;
				else
				{
					if (lock) careful_unlock();
					wait_write();
					goto retry;
				}
			else
				if (do_read(lock))
				{
					if (lock) careful_unlock();
					goto retry;
				}
		}
		// We have more than one cache remaining in list
		else
			if (do_read(lock))
			{
				if (lock)
					careful_unlock();
				goto retry;
			}
		if (lock)
			careful_unlock();
		return true;
	}

	bool read(T& t)
	{
		if (!safe_read(false))
			return safe_read(true);
		t = last_read;
		return true;
	}

	void do_write(const T& t, const bool locking)
	{
		if (writehead == CSP_CACHE_DEFAULT)
		{
			if (!locking)
				lockthis();

			std::array<T, CSP_CACHE_DEFAULT> adder;
			list.push_back(adder);
			listsiz++;
			writehead = 0;

			if (!locking)
				careful_unlock();
			// This is the optimal place to notify watching threads,
			//   according to tests
			wait_lock.notify_all();
		}
		list.back()[writehead++] = t;
	}
	void write(const T& t)
	{
		// Keep track if we are in a thread-safe mode
		bool locked = false;
		retry:
		if (!locked)
		{
			if (listsiz == 0 || listsiz == 1) // Not safe to add without lock
			{
				lockthis();
				locked = true;
				goto retry;
			}
			else
				do_write(t, false);
		}
		else
		{
			if (listsiz == 0)
			{
				list.resize(1);
				listsiz++;
				writehead = 0;
			}
			do_write(t, true);
			careful_unlock();
		}
	}

	void done()
	{
		finished = true;
		wait_lock.notify_all();
	}

	void lockthis()
	{
		flush.lock();
	}
	void careful_unlock()
	{
		flush.unlock();
	}

	void lock_read()
	{
		flush.lock();
	}
	void unlock_read()
	{
		flush.unlock();
	}
	void lock_write()
	{
		flush.lock();
	}
	void unlock_write()
	{
		flush.unlock();
		wait_lock.notify_all();
	}
	void wait_write()
	{
		wait_lock.wait(wait_lock_ml);
	}
};

}

#endif /* ONEWAY_CHANNEL_H_ */
