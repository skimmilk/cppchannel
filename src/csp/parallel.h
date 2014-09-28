/*
 * parallelize.h
 *
 *  Created on: Apr 16, 2014
 *      Author: skim
 */

#ifndef PARALLELIZE_H_
#define PARALLELIZE_H_

#include <csp/pipe.h>

/* ================================================================
 * parallel
 * Used to make the processing of input happen over multiple threads
 *   at the expense of making output unordered
 * Should only be used on channels whose input processing is slow
 * ================================================================
 */
template <typename t_in, typename t_out, typename... args>
class unordered_parallel_t : public
	csp::channel<
		t_in, t_out,  int, csp::channel<t_in,t_out,args...>*>
{
public:
	void run(int threadcount, csp::channel<t_in,t_out,args...>* channel)
	{
		std::vector<csp::channel<t_in,t_out,args...>> chans;

		chans.resize(threadcount);

		for (auto& a : chans)
		{
			if (!csp::is_nothing<t_out>::value)
				this->csp_output->always_lock = true;
			a.csp_output = this->csp_output;
			a.unique_output = false;
			a.csp_input = new csp::message_stream<t_in>();
			a.manage_input = true;

			a.arguments = channel->arguments;
			a.start = channel->start;

			a.start_background();
		}

		t_in current;
		int write_to_index = 0;
		while (this->read(current))
		{
			chans[write_to_index].csp_input->write(current);
			write_to_index++;
			write_to_index %= threadcount;
		}

		for (auto& a : chans)
		{
			a.csp_input->lock_this();
			a.csp_input->finished = true;
			a.csp_input->unlock_write();
			a.csp_input->done();
		}
	}
};
template <typename t_in, typename t_out, typename... t_args>
csp::channel<
	t_in, t_out, int,
	csp::channel<t_in,t_out,t_args...>*
	>
	parallel(int numthreads, csp::channel<t_in,t_out,t_args...>&& channel)
{
	static_assert(!csp::is_nothing<t_in>::value,
			"parallel needs an input channel");

	using type = csp::channel<
			t_in,t_out,int,csp::channel<t_in,t_out,t_args...>*>;

	type a;
	a.arguments = std::make_tuple(numthreads, &channel);
	a.start = (void(type::*)(int, csp::channel<t_in,t_out,t_args...>*))
			&unordered_parallel_t<t_in,t_out,t_args...>::run;
	return a;

}/* parallel */

/* ================================================================
 * schedule
 * For every input read,
 *   a channel is created with the value overloaded into its arguments
 * ================================================================
 */
template <typename t_in, typename t_out>
class schedule_t : public csp::channel<t_in,t_out>
{
public:
	void run(csp::channel<csp::nothing, t_out, t_in>(*functor)(t_in))
	{
		using created_channel_t = csp::channel<csp::nothing, t_out, t_in>;
		// Make sure writes are safe since there are multiple writers
		this->csp_output->always_lock = true;

		std::vector<created_channel_t*> channels;
		t_in in;
		while (this->read(in))
		{
			// Create the channel and set it up
			channels.push_back(new created_channel_t(functor(in)));
			channels.back()->unique_output = false;
			channels.back()->csp_output = this->csp_output;
			channels.back()->background = true;

			// Make channel run in background
			channels.back()->worker= std::thread(
					created_channel_t::begin_background,
					channels.back());
		}
		for (auto& w : channels)
			delete w;
	}
};
// Creation function
// functor passed is declared as
//   csp::channel<nothing,output,args...>(*name)(args...)
template <typename t_in, typename t_out>
csp::channel<t_in, t_out, csp::channel<csp::nothing, t_out, t_in>(*)(t_in)>
	schedule(csp::channel<csp::nothing, t_out, t_in>(*functor)(t_in))
{
	static_assert(!csp::is_nothing<t_in>::value,
			"schedule needs input");

	using type = csp::channel<
			t_in,t_out,decltype(functor)>;

	type a;
	a.arguments = std::make_tuple(functor);
	a.start = (void(type::*)(decltype(functor)))
			&schedule_t<t_in,t_out>::run;
	return a;
}/* schedule */

#endif /* PARALLELIZE_H_ */
