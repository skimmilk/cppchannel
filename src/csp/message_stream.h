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
#include <sys/syscall.h>
#include <unistd.h>

#include <csp/spaghetti.h>

namespace csp{

// Typical programming channel
// Blocks reading till stuff is written to the channel
// A stream will have readers and writers accessing through different threads
// This tries to not use locks when necessary, for performance
template<typename T>
class message_stream
{
private:
	// These are the indexes to where we are reading/writing in the cache
	uint32_t readhead, writehead;
	std::atomic<uint32_t> writehead_finished;

	// This is where the items are stored to be read
	// The linked list stores an array of data, the cache
	// The cache is used because the list must be locked
	//   every time it is written to and locking on every write is expensive
	// ===== -> ===== -> ===== -> ===-- -> 0
	typedef std::array<T, CSP_CACHE_DEFAULT> stream_cache;
	std::list<stream_cache> list;

	// Since the variable isn't atomic, and we want the length to be accurate
	// This mutex must be locked every time the list is written
	size_t list_size;
	std::mutex list_lock;

	// Used by wait_lock to block and resume
	std::mutex wait_lock_ml;

	// A thread is waiting for input in wait_write()
	std::atomic<bool> waiting;
public:
	T last_read;

	// Wait on this for input if we are waiting for writes
	std::condition_variable wait_lock;

	std::mutex read_lock;

	// This is a hint, namely from parallel.h functions,
	//   that this should always lock when writing items
	bool always_lock;

	std::atomic_bool finished;

	message_stream()
	{
		readhead = writehead = 0;
		writehead_finished = 0;
		always_lock = false;
		finished = false;
		list_size = 0;
		waiting = false;
	}

	// Returns true if there are items remaining in the list
	// Assumes mutex is locked
	bool items_remaining()
	{
		bool converged = (list_size == 1 && readhead == writehead_finished);
		if (finished)
			// Finished if list size is zero or reading/writing elements same
			return !(converged || list_size == 0);
		return true;
	}

	// Returns false if item could not be read safely
	// True on success
	bool read_list(bool locked)
	{
		if (readhead == CSP_CACHE_DEFAULT)
		{
			readhead = 0;
			if (!locked)
				lock_this();
			list.pop_front();
			list_size--;

			if (!locked)
				assert(list_size >= 1);

			if (!locked)
				unlock_this();
		}

		if (list_size == 0 || (list_size == 1 && readhead >= writehead))
			return false;

		last_read = list.front()[readhead++];
		return true;
	}
	// Returns true on success, false on failure
	// Will not read and fail if there are not enough entries in the list
	// The locked variable tells us if the list mutex is locked
	// If it is locked, we should not lock it again to prevent deadlocks
	bool do_read_simple(bool locked)
	{
		// The list is empty or may not fully populated
		// If the list is not fully populated we might read uncommitted items
		//   in the cache in the list
		if (list_size <= 2)
			return false;

		return read_list(locked);
	}
	// Reads items from the cache that may not be fully populated
	// Assumes mutex is locked, items remain, and list size is 1 or 0
	// Returns true if read success, false if no items remain in list
	bool do_read_tight()
	{
		if (list_size == 0 || readhead == writehead)
		{
			wait:
			unlock_this();
			wait_write();
			lock_this();

			if (!items_remaining())
			{
				unlock_this();
				return false;
			}
		}
		assert(list_size != 0);
		if (!read_list(true))
			goto wait;
		return true;
	}
	// Returns false if there are no items left
	bool safe_read(T& t)
	{
		lock_this();

		if (!do_read_simple(true))
		{
			// Are we finished?
			if (!items_remaining() || !do_read_tight())
			{
				unlock_this();
				return false;
			}
		}

		t = last_read;
		unlock_this();
		return true;
	}
	// Returns false if no items remaining to read
	bool read(T& t)
	{
		if (!do_read_simple(false))
			return safe_read(t);
		t = last_read;
		return true;
	}

	void do_write(const T& t, bool locked)
	{
		// Is the list empty?
		if (list_size == 0)
		{
			if (!locked)
				lock_this();
			list.resize(1);
			if (!locked)
				unlock_this();
			assert(writehead == 0);
		}
		// Is the cache full?
		if (writehead == CSP_CACHE_DEFAULT)
		{
			writehead = 0;

			// Add in another cache in the list
			if (!locked)
				lock_this();

			stream_cache tmp;
			list.push_back(tmp);
			list_size++;

			assert(list_size > 1);

			// Because readers might be waiting, we will want to message to
			//   those waiting every so often and now is a good opportunity
			if (!locked)
				unlock_write();
			else
				// We are locked but still want to message
				notify_readers();
		}
		// This go last because list_size gets updated during the lock
		// write() needs this updated variable so we don't have an old value
		//   and write to the front of the list
		list.back()[writehead++] = t;
	}
	// Will safely write the item to the list
	void safe_write(const T& t)
	{
		lock_this();

		// If the list is empty
		if (list_size == 0)
		{
			list.resize(1);
			list_size++;
			writehead = 0;
		}
		do_write(t, true);

		unlock_write();
	}
	// Returns true on success, false on failure
	// Write item to the stream
	void write(const T& t)
	{
		// Be sure to safely write when there are few items in the list
		//   as the front of the list will be written to
		//   and there might be a reader waiting
		if (always_lock || list_size <= 2)
			safe_write(t);
		else
			do_write(t, false);
	}

	void done()
	{
		writehead_finished = writehead;
		finished = true;
		while (waiting)
			wait_lock.notify_all();
	}

	void lock_this()
	{
		list_lock.lock();
	}
	void unlock_this()
	{
		list_lock.unlock();
	}
	void notify_readers()
	{
		if (waiting)
			wait_lock.notify_all();
	}
	void unlock_write()
	{
		list_lock.unlock();
		notify_readers();
	}
	void wait_write()
	{
		waiting = true;
		if (finished)
		{
			waiting = false;
			return;
		}

		// Halt until a writer unlocks us
		std::unique_lock<std::mutex> ul (wait_lock_ml);
		wait_lock.wait(ul);
		waiting = false;
	}
};

}

#endif /* ONEWAY_CHANNEL_H_ */
