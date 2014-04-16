/*
 * csplib.h
 *
 *  Created on: Apr 12, 2014
 *      Author: skim
 */

#ifndef CSPLIB_H_
#define CSPLIB_H_

#include <string>
#include <iostream>
#include <strings.h>

#include <csp/pipe.h>

namespace CSP{
/* ================================
 * cat
 * Writes a file out line by line
 * Pipes that get called with no input __MUST__ have CSP::nothing as input
 * ================================
 */
//       name     input       output     arguments     (arguments)
CSP_DECL(cat, CSP::nothing, std::string, std::string) (std::string file)
{
	std::string line;
	std::ifstream input(file);
	while (std::getline(input, line))
		put(line);
} // cat

/* ================================
 * to_lower
 * Outputs input, but in lower case
 * ================================
 */
CSP_DECL(to_lower, std::string, std::string) ()
{
	std::string line;
	while (read(line))
	{
		std::transform(line.begin(), line.end(), line.begin(), ::tolower);
		put(line);
	}
}// to_lower

/* ================================
 * sort
 * Sorts stuff
 * Call with sort<yourType>()
 * This is how you have to do templates with pipes
 * Sorting with heap allows us to sort while receiving input AND output
 * This optimizes the flow of the pipeline, more operations can happen at once
 *   push_heap is very fast -- average constant time
 *     Allows for O(log n) sorting time after input has finished
 *       (Output is also streamed while sorting)
 * ================================
 */
template <typename t_in>
class sort_t_: public CSP::csp_chan<t_in, t_in, CSP_CACHE_DEFAULT, bool>
{
public:
	void run(bool reverse)
	{
		std::vector<t_in> output;
		auto functor = reverse? [](t_in& a, t_in&b){ return a < b; }
					: [](t_in& a, t_in& b){ return a > b; };

		std::make_heap(output.begin(), output.end(), functor);
		t_in line;

		while (this->read(line))
		{
			output.push_back(line);
			std::push_heap(output.begin(), output.end(), functor);
		}

		//std::sort_heap(output.begin(), output.end(), functor);
		int siz = output.size();
		for (int i = 0; i < siz; i++)
		{
			this->put(output.front());
			std::pop_heap(output.begin(), output.end() - i, functor);
		}
	}
};
template <typename t_in>
CSP::csp_chan<t_in, t_in, CSP_CACHE_DEFAULT, bool> sort(bool a = false)
{
	return CSP::csp_chan_create<
			t_in, t_in, sort_t_<t_in>, CSP_CACHE_DEFAULT, bool>(a);
}/* sort */

/* ================================
 * grab
 * Writes out strings that contain the specified string
 * ================================
 */
CSP_DECL(grab, std::string, std::string, std::string, bool)
								(std::string search, bool invert)
{
	std::string current_line;
	while (read(current_line))
		// Put string if the currently line contains search
		if ((current_line.find(search) == std::string::npos) == invert)
			put(current_line);
} // grab

/* ================================
 * Generic uniq
 * Works well with sort
 * Only removes adjacent equivalent lines
 * ================================
 */
template <typename t_in> class uniq_t : public CSP::csp_chan<t_in,t_in>
{
public:
	void run()
	{
		// First, read one
		t_in current, last;
		if (!this->read(last))
			return;
		this->put(last);
		while(this->read(current))
		{
			if (current != last)
				this->put(current);
			last = current;
		}
	}
};
template <typename t_in> CSP::csp_chan<t_in, t_in> uniq()
{
	return CSP::csp_chan_create<
			t_in, t_in, uniq_t<t_in>>();
}/* uniq */

/* ================================
 * print
 * Prints input stream to stdout
 * ================================
 */
CSP_DECL(print, std::string, CSP::nothing)()
{
	std::string line;
	while (read(line))
		std::cout << line + "\n";
} // print
// Prints to stderr
CSP_DECL(print_log, std::string, CSP::nothing)()
{
	std::string line;
	while (read(line))
		std::cerr << line + "\n";
} // print_log

/* ================================
 * vec
 * Sends out items in a vector over a pipe
 *  vector<string> a;
 *  ...
 *  vec(string) | print();
 *  ================================
 */
// vec has a very fast output
// To avoid spamming thread locks uselessly, write less often
#define CSP_CAT_CACHE (CSP_CACHE_DEFAULT*4)
template <typename t_in>
class cat_generic : public
	CSP::csp_chan<CSP::nothing, t_in, CSP_CAT_CACHE, std::vector<t_in>*>
{
public:
	void run(std::vector<t_in>* kitty)
	{
		for (auto& a : *kitty)
			this->put(a);
	}
};
template <typename t_in>
CSP::csp_chan<CSP::nothing, t_in, CSP_CAT_CACHE, std::vector<t_in>*>
		vec(std::vector<t_in>& kitty)
{
	csp_chan<CSP::nothing, t_in, CSP_CAT_CACHE, std::vector<t_in>*> a;
	a.arguments = std::make_tuple(&kitty);
	// Fun fact: I figured out how to type this line
	//  due to helpful compiler errors
	a.start = (void(csp_chan<CSP::nothing,t_in,CSP_CAT_CACHE,std::vector<t_in>*>::*)
			(std::vector<t_in>*))&cat_generic<t_in>::run;

	return a;

}/* vec */

/* ================================
 * chan_read
 * Iterate over a pipe in-line
 * cat(file) | chan_read<...>...
 * ================================
 */
template <typename t_in, typename t_out>
class chan_read_t : public CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		std::function<void(CSP::csp_chan<t_in, t_out>*, t_in&)>>
{
public:
	// This is the type we want to be, a CSP pipe
	using thistype = CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		std::function<void(CSP::csp_chan<t_in, t_out>*, t_in&)>>;

	void run(std::function<void(thistype*,t_in&)> a)
	{
		t_in line;
		while (this->read(line))
			a(this, line);
	}
};

template <typename t_in, typename t_out>
CSP::csp_chan
<
	t_in,
	t_out,
	CSP_CACHE_DEFAULT,
	std::function
	<
		void(CSP::csp_chan<t_in,t_out>*, t_in&)
	>
>
	chan_read(
		std::function<void(CSP::csp_chan<t_in, t_out>*, t_in&)>
					asdf)
{
	// madotsuki_eating_soup.jpg
	using thistype = CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		std::function<void(CSP::csp_chan<t_in, t_out>*, t_in&)>>;

	thistype result;

	result.arguments = std::make_tuple(asdf);
	result.start = (void (thistype::*)
					(std::function<void(CSP::csp_chan<t_in,t_out>*,t_in&)>)
		)&chan_read_t<t_in,t_out>::run;

	return result;
} // chan_read

/* ================================
 * chan_read
 * its chan_read without sending thisptr
 * (can't use thisptr->put() -- thus no output)
 * ================================
 */
template <typename t_in>
class chan_sans_output_read_t : public CSP::csp_chan<
		t_in, CSP::nothing, CSP_CACHE_DEFAULT,
		std::function<void(t_in&)>>
{
public:
	// This is the type we want to be, a CSP pipe
	using thistype = CSP::csp_chan<
		t_in, CSP::nothing, CSP_CACHE_DEFAULT,
		std::function<void(t_in&)>>;

	void run(std::function<void(t_in&)> a)
	{
		t_in line;
		while (this->read(line))
			a(line);
	}
};
template <typename t_in>
CSP::csp_chan
<
	t_in,
	CSP::nothing,
	CSP_CACHE_DEFAULT,
	std::function
	<
		void(t_in&)
	>
>
	chan_read(
		std::function<void(t_in&)>
					asdf)
{
	// madotsuki_eating_soup.jpg
	using thistype = CSP::csp_chan<
		t_in, CSP::nothing, CSP_CACHE_DEFAULT,
		std::function<void(t_in&)>>;

	thistype result;

	result.arguments = std::make_tuple(asdf);
	result.start = (void (thistype::*)
					(std::function<void(t_in&)>)
		)&chan_sans_output_read_t<t_in>::run;

	return result;
} // chan_read


}; /* namespace CSP */

#endif /* CSPLIB_H_ */
