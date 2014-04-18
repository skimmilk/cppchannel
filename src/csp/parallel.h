/*
 * parallelize.h
 *
 *  Created on: Apr 16, 2014
 *      Author: skim
 */

#ifndef PARALLELIZE_H_
#define PARALLELIZE_H_

#include <csp/pipe.h>

/* parallel
 * Used to make the processing of input happen over multiple threads
 *   at the expense of making output unordered
 * Should only be used on channels whose input processing is slow
 */
template <typename t_in, typename t_out, int cache, typename... args>
class unordered_parallel_t : public
	CSP::csp_chan<
		t_in, t_out, cache, int, CSP::csp_chan<t_in,t_out,cache,args...>*>
{
public:
	void run(int threadcount, CSP::csp_chan<t_in,t_out,cache,args...>* chan)
	{
		std::vector<CSP::csp_chan<t_in,t_out,cache,args...>> chans;
		//std::vector<CSP::csp_message_stream<t_in>> inputs;

		chans.resize(threadcount);

		for (auto& a : chans)
		{
			a.csp_output = this->csp_output;
			a.unique_output = false;
			//a.csp_input = &thisinput;
			a.csp_input = new CSP::csp_message_stream<t_in>();

			a.arguments = chan->arguments;
			a.start = chan->start;

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
CSP::csp_chan<
	t_in, t_in, CSP_CACHE_DEFAULT, int,
	CSP::csp_chan<t_in,t_out,cache,t_args...>*
	>
	parallel(int numthreads, CSP::csp_chan<t_in,t_out,cache,t_args...>&& chan)
{
	static_assert(!CSP::is_nothing<t_in>::value,
			"parallel needs an input channel");

	/*return CSP::csp_chan_create<
			t_in, t_in, unordered_parallel_t<t_in,t_out,cache>, cache,
			int, CSP::csp_chan<t_in,t_out,cache,t_args...>*>
				(numthreads, &chan);*/

	using type = CSP::csp_chan<
			t_in,t_out,cache,int,CSP::csp_chan<t_in,t_out,cache,t_args...>*>;

	type a;
	a.arguments = std::make_tuple(numthreads, &chan);
	// Fun fact: I figured out how to type this line
	//  due to helpful compiler errors
	a.start = (void(type::*)(int, CSP::csp_chan<t_in,t_out,cache,t_args...>*))
			&unordered_parallel_t<t_in,t_out,cache,t_args...>::run;
	return a;

}/* sort */


#endif /* PARALLELIZE_H_ */
