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
#include <csp/read.h>

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
class sort_t_: public CSP::channel<t_in, t_in, bool>
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

		int siz = output.size();
		for (int i = 0; i < siz; i++)
		{
			this->put(output.front());
			std::pop_heap(output.begin(), output.end() - i, functor);
		}
	}
};
template <typename t_in>
CSP::channel<t_in, t_in, bool> sort(bool a = false)
{
	return CSP::chan_create<
			t_in, t_in, sort_t_<t_in>, bool>(a);
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
template <typename t_in> class uniq_t : public CSP::channel<t_in,t_in>
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
template <typename t_in> CSP::channel<t_in, t_in> uniq()
{
	return CSP::chan_create<
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
template <typename t_in>
class cat_generic : public
	CSP::channel<CSP::nothing, t_in, std::vector<t_in>*>
{
public:
	void run(std::vector<t_in>* kitty)
	{
		for (auto& a : *kitty)
			this->put(a);
	}
};
template <typename t_in>
CSP::channel<CSP::nothing, t_in, std::vector<t_in>*>
		vec(std::vector<t_in>& kitty)
{
	channel<CSP::nothing, t_in, std::vector<t_in>*> a;
	a.arguments = std::make_tuple(&kitty);
	// Fun fact: I figured out how to type this line
	//  due to helpful compiler errors
	a.start = (void(
			channel<CSP::nothing,t_in,std::vector<t_in>*>::*)
		(std::vector<t_in>*))&cat_generic<t_in>::run;

	return a;

}/* vec */

}; /* namespace CSP */

#endif /* CSPLIB_H_ */
