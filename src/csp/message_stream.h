/*
 * oneway_channel.h
 *
 *  Created on: Apr 13, 2014
 *      Author: skim
 */

#ifndef ONEWAY_CHANNEL_H_
#define ONEWAY_CHANNEL_H_

namespace CSP{

// Typical programming channel
// Blocks reading till stuff is written
// Never blocks writes!
// Cache isn't implemented here because it would require the holder
//  of input and output channels to know the cache sizes of each other
template<typename T>
class csp_message_stream : public std::vector<T>
{
public:
	// Lock this when reading/writing
	std::mutex lock;
	// Lock this to wait for lock to lock...
	//  i.e. This is a message that lock has been touched
	std::mutex antilock;

	bool finished;

	csp_message_stream()
	{
		finished = false;
	}
	~csp_message_stream()
	{
		finished = true;
	}

	// Do the actual read, just the read
	void do_read(std::vector<T>& outvec)
	{
		int siz = this->size();
		for (int i = 0; i < siz; i++)
			//outvec[i] = this->at(i);
			outvec.push_back(this->at(i));
	}
	// Read as much as we can INTO buffer
	// Need to lock because writing could modify the buffer's location
	int read(std::vector<T>& buffer)
	{
		retry_read:
		lock.lock();
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
			wait_write();
			goto retry_read;
		}

		this->clear();
		lock.unlock();
		return siz;
	}

	// Write out whole array to this
	template <int amt>
	void write(const std::array<T, amt>& a, const bool do_lock = true)
	{
		if (do_lock)
			lock_write();
		for (auto& b : a) this->push_back(b);
		if (do_lock)
			unlock_write();
	}
	// Write out the first amount of stuff to this
	template <int amt>
	void write(const std::array<T, amt>& a, int amount, const bool do_lock = true)
	{
		if (do_lock)
			lock_write();
		for (int i = 0; i < amount; i++)
			this->push_back(a[i]);
		if (do_lock)
			unlock_write();
	}

private:
	void lock_write()
	{
		lock.lock();
	}
	void unlock_write()
	{
		lock.unlock();
		antilock.unlock();
	}
	void wait_write()
	{
		// Double lock ensures we'll wait till unlock_write gets called
		// Not the best way to do this, but safe enough
		lock.unlock();
		// Flow gets stopped for sure here, and will continue
		//  once antiLock gets unlocked
		// Make sure this doesn't double-wait-lock on this
		if (antilock.try_lock())
			antilock.lock();
		else
			antilock.lock();
	}
};

}

#endif /* ONEWAY_CHANNEL_H_ */
