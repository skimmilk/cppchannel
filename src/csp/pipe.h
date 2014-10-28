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

template<typename T>
struct shared_ptr;

struct nothing { char a; };
template<typename T> struct is_nothing { static const bool value = false; };
template<> struct is_nothing<nothing> { static const bool value = true; };

template <typename t_in, typename t_out, typename... t_args>
class channel
{
	using this_pipe = channel<t_in,t_out,t_args...>;
public:
	// Wait to write till this is filled
	std::thread worker;

	// True if work is done in the background
	bool background;
	// True if input/output must be deleted
	bool manage_input;
	bool manage_output;

	message_stream<t_in>* csp_input;
	message_stream<t_out>* csp_output;

	// The channel that contains the pipeline information
	channel* master;
	// This references all of the channels in the pipeline to keep them alive
	//   through their shared_ptr
	// It enables the pipeline to be tasked in the background, instead of
	//   being blocked
	// For example, in the line: a() | b() | c()
	// Two of the functors must be deleted before returning, because
	//   the operator |'s must return only one channel
	std::vector<csp::shared_ptr<this_pipe>> pipeline;

	// The function pointer member to the function that this channel runs
	void(this_pipe::*start)(t_args...) = 0;

	// Variable length...
	// Putting this last allows for self-referential pipes to be called
	//   through a pointer without knowing the size of the member to align
	//   the other members, because there are no members below this variable
	// Specifically fixes chan_read
	std::tuple<t_args...> arguments;

	channel() : background(false), manage_input(false), manage_output(true),
			csp_input(NULL), csp_output(NULL), master(NULL)
	{
		if (!is_nothing<t_out>::value)
			csp_output = new message_stream<t_out>();
	}
	// Wait to finish first, then exit to self-destruct
	~channel()
	{
		if (background)
			worker.join();

		pipeline.clear();

		if (!is_nothing<t_out>::value && manage_output)
			delete csp_output;
		if (!is_nothing<t_in>::value && manage_input)
			delete csp_input;
	}
	// Copy constructor, needed for csp_create...
	channel(const this_pipe& src)
	{
		manage_input = src.manage_input;
		manage_output = src.manage_output;
		csp_output = src.csp_output;
		csp_input = src.csp_input;
		background = src.background;
		start = src.start;
		master = src.master;
	}

	void put(const t_out& out)
	{
		csp_output->write(out);
	}
	void put(t_out&& out)
	{
		csp_output->write(std::forward<t_out>(out));
	}

	bool read(t_in& input)
	{
		static_assert(!is_nothing<t_in>::value,
				"Called read in csp pipe without input");
		spinlock(&csp_input);

		return csp_input->read(input);
	}
	// Needed if multiple threads accessing this channel
	bool safe_read(t_in& input)
	{
		static_assert(!is_nothing<t_in>::value,
				"Called read in csp pipe without input");
		spinlock(&csp_input);

		return csp_input->safe_read(input);
	}

	// Only call this if the channel was created through encap()
	// This will message that this input is complete
	void close_input()
	{
		csp_output->lock_this();
		csp_output->done();
		csp_output->unlock_this();
	}

	/* DO NOT TOUCH */
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

	// Called by worker thread only, runs the runner
	static void begin_background(this_pipe* a)
	{
		a->do_start();
	}

	template<typename T>
	void spinlock(message_stream<T>** waiton)
	{
		std::mutex barrier;
		while (*waiton == NULL)
		{
			barrier.lock();
			if (*waiton) {barrier.unlock(); break;}
			barrier.unlock();
			pthread_yield();
		}
	}
public:
	// Starting point to finally do all the work
	bool do_start()
	{
		// Create a tuple with the this pointer and arguments together
		// Works with the tuple expander call function easily
		std::tuple<channel*> head (this);
		std::tuple<channel*, t_args...> thisargs =
				std::tuple_cat(head, arguments);
		call(do_start_actually, thisargs);

		// Finished processing input
		if (!is_nothing<t_out>::value && manage_output)
			csp_output->done();
		return true;
	}
	void start_background()
	{
		background = true;
		worker = std::thread(begin_background, this);
	}
};

// Template to help dereference types
template<typename s>
struct dereference { typedef s type; };
template<typename s>
struct dereference<s*> { typedef typename dereference<s>::type type; };

template<typename T>
struct shared_ptr : std::shared_ptr<T>
{
	/* ========================
	 * operator |
	 * links channels together
	 * returns the rightmost pipe when strung together
	 * ========================
	 */
	template <typename ot_in,typename ot_out,typename...ot_args>
	csp::shared_ptr<channel<ot_in, ot_out, ot_args...>>&
	operator |(csp::shared_ptr<channel<ot_in,ot_out,ot_args...>>&& pipe)
	{
		// Force all writes to RAM
		std::mutex a;
		a.lock();
		// this = left, pipe = right
		pipe->csp_input = this->get()->csp_output;
		a.unlock();

		auto thispipe = this->get();
		using pipe_type = channel<ot_in,ot_out,ot_args...>*;

		if (thispipe->master == NULL)
			thispipe->master = thispipe;

		// Copy the master's stuff to the pipe and set the pipe as the master
		pipe->master = pipe.get();

		thispipe->master->pipeline.push_back(*this);
		pipe->pipeline = ((pipe_type)thispipe)->master->pipeline;
		thispipe->master->pipeline.clear();

		thispipe->master = (decltype(this->get()))pipe.get();

		// Right hand side doesn't output?
		// Block further execution so we don't run over statements like
		// cat() | print()
		if (is_nothing<ot_out>::value)
		{
			pipe->background = false;
			this->get()->start_background();
			pipe->do_start();
		}
		else
			this->get()->start_background();

		return pipe;
	}
};

template<typename T>
shared_ptr<T> make_shared()
{
	auto a = std::make_shared<T>();
	return *(shared_ptr<T>*)&a;
}

/* ========================
 * chan_create
 * creates the channel, used in the declaration of channel functors
 * ========================
 */
template <typename t_in, typename t_out, typename holder,
typename... t_args>
static csp::shared_ptr<channel<t_in, t_out, t_args...>>
chan_create(t_args... args)
{
	using this_pipe = channel<t_in,t_out,t_args...>;

	auto a = csp::make_shared<this_pipe>();
	a->arguments = std::make_tuple(args...);
	// Fun fact: I figured out how to type this line
	//  due to helpful compiler errors
	a->start = (void(this_pipe::*)(t_args...))&holder::run;
	return a;
}

// The pipe-into operator must be >>= because >> gets executed before |
// This causes statements cat(file) | grab("stuff") >> vectorOfStrings
//   to error
template <typename tin, typename tout, typename... targs>
csp::shared_ptr<channel<tin, tout, targs...>>&
operator >>=(csp::shared_ptr<channel<tin,tout,targs...>>& in,
		std::vector<tout>& out)
{
	// Since this has to be executed last
	//  this is the function that will block the calling thread
	//  and wait for everything to finish
	// So we execute everything in this thread instead of a background one
	in->background = false;
	in->do_start();
	tout a;
	while (in->csp_output->read(a))
		out.push_back(std::move(a));
	return in;
}

/* ========================
 * encap
 * Takes in a pipeline, returns a channel which writes to the
 *   front of the pipeline and reads the end of the pipeline
 * ========================
 */
template <typename tin, typename tout,
		typename otin, typename otout, typename... otargs>
csp::channel<tin, tout>
	encap(csp::shared_ptr<csp::channel<otin, otout, otargs...>>& pipe)
{
	csp::channel<tin,tout> result;

	std::mutex barrier;
	barrier.lock();

	// Merge the input/output with result's
	// Users read this channel from csp_input, write to csp_output
	// Leftmost is written to, rightmost is read from
	pipe->pipeline.front()->csp_input = result.csp_output;
	result.csp_input = pipe->csp_output;

	// There exists a race condition when calling put() and close_input()
	// Must lock when writing and closing
	result.csp_input->always_lock = true;

	barrier.unlock();

	pipe->start_background();

	// Put the master pipe into this vector so it stays referenced
	//   until this encapsulation gets destroyed
	auto push = (decltype(result.pipeline.front()))pipe;
	result.pipeline.push_back(push);

	return result;
}

} /* namespace csp */

#endif /* CSP_H_ */
