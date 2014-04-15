/*
 * csp.h
 *
 *  Created on: Apr 10, 2014
 *      Author: skim
 */

#ifndef CSP_H_
#define CSP_H_

#include <vector>
#include <thread>
#include <future>
#include <mutex>
#include <algorithm>
#include <tr1/tuple>

#include "spaghetti.h"
#include "message_stream.h"

namespace CSP
{

struct nothing { char a; };
template<typename T> struct is_nothing { static const bool value = false; };
template<> struct is_nothing<nothing> { static const bool value = true; };

template <typename t_in, typename t_out, int cachesiz = 64, typename... t_args>
class csp_pipe
{
	using this_pipe = csp_pipe<t_in,t_out,cachesiz,t_args...>;
private:
	// Wait to write till this is filled
	std::array<t_out, cachesiz> cache_write;
	std::vector<t_in> cache_read;
	int cache_read_head;
	int cache_write_head;

	std::thread worker;

public:
	bool background;
	csp_message_stream <t_in>* csp_input;
	csp_message_stream <t_out> csp_output;

	// Locked till left-hand side has initialized
	// stuff evaluates left-to-right, so
	// cat(stuff) | sort() | uniq()
	// uniq gets called first, goes into background immediately
	// then waits for this lock to unlock so it doesn't read garbage csp_input
	std::mutex wait_lock;
	std::mutex* next_node_lock;

	// Variable length...
	// Putting this last allows for self-referential pipes to be called
	//   through a pointer without knowing the size of the member to align
	//   the other members, because there are no members below this variable
	// Specifically fixes pipe_read
	std::tuple <t_args...> arguments;

	csp_pipe()
	{
		background = false;
		cache_write_head = 0;
		csp_input = 0;
		cache_read_head = 0;

		// Lock here because constructor gets called first
		wait_lock.lock();
		next_node_lock = NULL;
	}
	// Wait to finish first, then exit to self-destruct
	~csp_pipe()
	{
		if (background)
			worker.join();
	}

	void flush_cache()
	{
		csp_output.template write<cachesiz>(cache_write);
		cache_write_head = 0;
	}

	void put(const t_out& out)
	{
		if (cache_write_head == cachesiz)
			flush_cache();
		cache_write[cache_write_head] = out;
		cache_write_head++;
	}

	// The input program finished writing
	// Nothing to read and is finished
	bool eof()
	{
		return cache_read.size() <= cache_read_head && csp_input->size() == 0 && csp_input->finished;
	}

	bool read(t_in& input)
	{
		if (sizeof(t_in) != 0 && csp_input == 0)
			throw std::runtime_error("Called read in CSP pipe without input");
		if (!eof())
		{
			// If our local read cache is empty, request another one
			if (cache_read.size() > cache_read_head)
				input = cache_read[cache_read_head++];
			else
			{ // Empty, gotta read more
				cache_read.clear();
				if (csp_input->read(cache_read) == 0)
					return false;
				cache_read_head = 1;
				input = cache_read[0];
			}
			return true;
		}
		return false;
	}

	// Copy constructor, won't compile without this
	// Needed for csp_create...
	csp_pipe(const csp_pipe& src)
	{
		csp_input = src.csp_input;
		cache_write_head = src.cache_write_head;
		cache_read_head = src.cache_read_head;
		background = src.background;
		next_node_lock = src.next_node_lock;
	}
	/* DO NOT TOUCH */
public:
	void(this_pipe::*start)(t_args...)= 0;
private:
	void do_start_actually_really(t_args&... a)
	{
		(this->*start)(a...);
	}
	static void do_start_actually(this_pipe* thisptr, t_args&... a)
	{
		thisptr->do_start_actually_really(a...);
	}
	/* END DO NOT TOUCH */

public:
	// Starting point to finally do all the work
	bool do_start()
	{
		// If the input is nothing, don't wait lock for input
		if (!is_nothing<t_in>::value)
			wait_lock.lock();
		if (next_node_lock)
			next_node_lock->unlock();

		// Create a tuple with the this pointer and arguments together
		// Works with the tuple expander call function easily
		std::tuple<csp_pipe*> head (this);
		std::tuple<csp_pipe*, t_args...> thisargs =
				std::tuple_cat(head, arguments);
		call(do_start_actually, thisargs);

		// Clean up
		if (!is_nothing<t_out>::value)
		{
			csp_output.lock.lock();
			csp_output.template write<cachesiz>(cache_write, cache_write_head, false);
			csp_output.finished = true;
			csp_output.lock.unlock();
			csp_output.antilock.unlock();
		}
		return true;
	}
private:
	// Called by worker thread only, runs the runner
	static void begin_background(this_pipe* a)
	{
		a->do_start();
	}

public:
	// The pipe | operator
	template <typename ot_in,typename ot_out,int ocachesiz,typename...ot_args>
	csp_pipe<ot_in, ot_out, ocachesiz, ot_args...>&
		operator |(csp_pipe<ot_in,ot_out,ocachesiz,ot_args...>&& pipe)
	{
		// this = left, pipe = right
		background = true;
		next_node_lock = &pipe.wait_lock;
		pipe.background = true;
		pipe.csp_input = &csp_output;

		// Right hand side doesn't output?
		// Block further execution so we don't run over statements like
		// cat() | print()
		if (is_nothing<ot_out>::value)
		{
			pipe.background = false;
			worker = std::thread(begin_background, this);
			pipe.do_start();
		}
		else
			worker = std::thread(begin_background, this);

		return pipe;
	}

	// The pipe-into operator must be >>= because >> gets executed before |
	// This causes statements cat(file) | grab("stuff") >> vectorOfStrings
	//   to error
	this_pipe& operator >>=(std::vector<t_out>& out)
	{
		// Since this has to be executed last
		//  this is the function that will block the calling thread
		//  and wait for everything to finish
		// So we execute everything in this thread instead of a background one
		background = false;
		do_start();
		out = csp_output;
		return *this;
	}

	// Force the calling thread to block on this
	//  instead of going into the background
	// Not sure how to implement this
private:
	this_pipe& block()
	{
		return *this;
	}
};

template <typename t_in, typename t_out, typename holder, int cachesiz = 64,
		typename... t_args>
static csp_pipe<t_in, t_out, cachesiz, t_args...>
		csp_pipe_create(t_args... args)
{
	csp_pipe<t_in, t_out, cachesiz, t_args...> a;
	a.arguments = std::make_tuple(args...);
	// Fun fact: I figured out how to type this line
	//  due to helpful compiler errors
	a.start = (void(csp_pipe<t_in,t_out,cachesiz,t_args...>::*)
												(t_args...))&holder::run;
	return a;
}

} /* namespace csp */

#endif /* CSP_H_ */
