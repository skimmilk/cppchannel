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

#include <csp/spaghetti.h>

namespace CSP{

// Typical programming channel
// Blocks reading till stuff is written
// Never blocks writes!
// Cache isn't implemented here because it would require the holder
//  of input and output channels to know the cache sizes of each other
template<typename T>
class message_stream : public std::vector<T>
{
private:
	std::mutex wait_lock_m;
	// Locked when reading/writing
	std::mutex flush;
	std::unique_lock<std::mutex> wait_lock_ml;
public:
	// Wait on this for input
	std::condition_variable wait_lock;

	bool finished;

	message_stream()
	{
		finished = false;
		wait_lock_ml = std::unique_lock<std::mutex>(wait_lock_m);
	}

	// Do the actual read, just the read
	void do_read(std::vector<T>& outvec)
	{
		int siz = this->size();
		for (int i = 0; i < siz; i++)
			outvec.push_back(this->at(i));
	}
	// Read as much as we can INTO buffer
	// Need to lock because writing could modify the buffer's location
	int read(std::vector<T>& buffer)
	{
		retry_read:
		lock_read();
		size_t siz = this->size();

		if (finished)
		{
			if (siz)
				do_read(buffer);
		}
		else if (siz)
			do_read(buffer);
		else
		{
			unlock_read();
			wait_write();
			goto retry_read;
		}

		this->clear();
		unlock_read();

		return siz;
	}

	// Write out whole array to this
	template <int amt>
	void write(const std::array<T, amt>& a, bool do_lock = true)
	{
		if (do_lock)
			lock_write();
		for (auto& b : a) this->push_back(b);
		if (do_lock)
			unlock_write();
	}
	// Write out the first amount of stuff to this
	void write(T* a, int amount, bool do_lock = true)
	{
		if (do_lock)
			lock_write();
		for (int i = 0; i < amount; i++)
			this->push_back(a[i]);
		if (do_lock)
			unlock_write();
	}

	void write(std::vector<T>& a, bool do_lock = true)
	{
		if (do_lock)
			lock_write();
		for (auto& b : a) this->push_back(b);
		if (do_lock)
			unlock_write();
	}

	void done()
	{
		wait_lock.notify_all();
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
