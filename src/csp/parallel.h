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
template <typename t_in, typename t_out, int cache, typename... args>
class unordered_parallel_t : public
	CSP::channel<
		t_in, t_out, cache, int, CSP::channel<t_in,t_out,cache,args...>*>
{
public:
	void run(int threadcount, CSP::channel<t_in,t_out,cache,args...>* channel)
	{
		std::vector<CSP::channel<t_in,t_out,cache,args...>> chans;

		chans.resize(threadcount);

		for (auto& a : chans)
		{
			a.csp_output = this->csp_output;
			a.unique_output = false;
			a.csp_input = new CSP::message_stream<t_in>();

			a.arguments = channel->arguments;
			a.start = channel->start;

			a.start_background();
		}

		t_in current;
		std::vector<t_in> input_cache;
		int write_to_index = 0;
		while (this->read(current))
		{
			input_cache.push_back(current);
			if (input_cache.size() == cache)
			{
				chans[write_to_index].csp_input->write(input_cache);
				write_to_index++;
				write_to_index %= threadcount;
				input_cache.clear();
			}
		}
		// Empty out cache
		if (input_cache.size())
			chans[write_to_index].csp_input->write(input_cache);

		for (auto& a : chans)
		{
			a.csp_input->lock_write();
			a.csp_input->finished = true;
			a.csp_input->unlock_write();
			a.csp_input->done();
		}
	}
};
template <typename t_in, typename t_out, int cache, typename... t_args>
CSP::channel<
	t_in, t_in, CSP_CACHE_DEFAULT, int,
	CSP::channel<t_in,t_out,cache,t_args...>*
	>
	parallel(int numthreads, CSP::channel<t_in,t_out,cache,t_args...>&& channel)
{
	static_assert(!CSP::is_nothing<t_in>::value,
			"parallel needs an input channel");

	using type = CSP::channel<
			t_in,t_out,cache,int,CSP::channel<t_in,t_out,cache,t_args...>*>;

	type a;
	a.arguments = std::make_tuple(numthreads, &channel);
	a.start = (void(type::*)(int, CSP::channel<t_in,t_out,cache,t_args...>*))
			&unordered_parallel_t<t_in,t_out,cache,t_args...>::run;
	return a;

}/* parallel */

#endif /* PARALLELIZE_H_ */
