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
#include <algorithm>
#include <cassert>

#include <csp/message_stream.h>

namespace csp
{

struct nothing { char a; };
template<typename T> struct is_nothing { static const bool value = false; };
template<> struct is_nothing<nothing> { static const bool value = true; };

template <typename t_in, typename t_out, typename... t_args>
class channel
{
	using this_pipe = channel<t_in,t_out,t_args...>;
private:
	// Wait to write till this is filled
	std::thread worker;

public:
	bool background;
	bool manage_input;
	message_stream<t_in>* csp_input;

	bool unique_output;
	message_stream<t_out>* csp_output;

	// Variable length...
	// Putting this last allows for self-referential pipes to be called
	//   through a pointer without knowing the size of the member to align
	//   the other members, because there are no members below this variable
	// Specifically fixes chan_read
	std::tuple<t_args...> arguments;

	channel()
	{
		manage_input = false;
		unique_output = true;
		if (!is_nothing<t_out>::value)
			csp_output = new message_stream<t_out>();

		background = false;
		csp_input = 0;
	}
	// Wait to finish first, then exit to self-destruct
	~channel()
	{
		if (background)
			worker.join();

		if (!is_nothing<t_out>::value && unique_output)
			delete csp_output;
		if (!is_nothing<t_in>::value && manage_input)
			delete csp_input;
	}

	void put(const t_out& out)
	{
		csp_output->write(out);
	}
	// Needed if multiple threads accessing this channel
	void safe_put(const t_out& out)
	{
		csp_output->safe_write(out);
	}

	bool read(t_in& input)
	{
		static_assert(!is_nothing<t_in>::value,
				"Called read in csp pipe without input");

		// Simple spinlock, wait for input to update
		while (!csp_input)
		{
			std::mutex barrier;
			barrier.lock();
			if (csp_input) {barrier.unlock(); break;}
			barrier.unlock();
		}

		return csp_input->read(input);
	}
	// Needed if multiple threads accessing this channel
	bool safe_read(t_in& input)
	{
		static_assert(!is_nothing<t_in>::value,
				"Called read in csp pipe without input");

		while (!csp_input)
		{
			std::mutex barrier;
			barrier.lock();
			if (csp_input) {barrier.unlock(); break;}
			barrier.unlock();
		}

		return csp_input->safe_read(input);
	}

	// Copy constructor, won't compile without this
	// Needed for csp_create...
	// Never actually gets called just put stuff here to stop compiler warnings
	channel(const channel& src)
	{
		manage_input = src.manage_input;
		unique_output = src.unique_output;
		csp_output = src.csp_output;
		csp_input = src.csp_input;
		background = src.background;
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
		assert(is_nothing<t_in>::value || csp_input != 0);
		// Create a tuple with the this pointer and arguments together
		// Works with the tuple expander call function easily
		std::tuple<channel*> head (this);
		std::tuple<channel*, t_args...> thisargs =
				std::tuple_cat(head, arguments);
		call(do_start_actually, thisargs);

		// Finished processing input
		if (!is_nothing<t_out>::value && unique_output)
			csp_output->done();
		return true;
	}
	void start_background()
	{
		background = true;
		worker = std::thread(begin_background, this);
	}
private:
	// Called by worker thread only, runs the runner
	static void begin_background(this_pipe* a)
	{
		a->do_start();
	}

public:
	// The pipe | operator
	template <typename ot_in,typename ot_out,typename...ot_args>
	channel<ot_in, ot_out, ot_args...>&
		operator |(channel<ot_in,ot_out,ot_args...>&& pipe)
	{
		// Force all writes to RAM
		std::mutex a;
		a.lock();

		// this = left, pipe = right
		background = true;
		pipe.background = true;
		pipe.csp_input = csp_output;

		a.unlock();

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
		t_out& a = csp_output->last_read;
		while (csp_output->read(a))
			out.push_back(a);
		return *this;
	}
};

template <typename t_in, typename t_out, typename holder,
		typename... t_args>
static channel<t_in, t_out, t_args...>
		chan_create(t_args... args)
{
	using this_pipe = channel<t_in,t_out,t_args...>;

	this_pipe a;
	a.arguments = std::make_tuple(args...);
	// Fun fact: I figured out how to type this line
	//  due to helpful compiler errors
	a.start = (void(this_pipe::*)(t_args...))&holder::run;
	return a;
}

} /* namespace csp */

#endif /* CSP_H_ */
