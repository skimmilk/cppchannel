/*
 * read.h
 *
 *  Created on: Apr 16, 2014
 *      Author: skim
 */

#ifndef READ_H_
#define READ_H_

#include <csp/csplib.h>

namespace CSP{

/* ================================================================
 * chan_readwrite
 * Iterate over a pipe in-line, selectively output
 * cat(file) | chan_read<...>...
 * ================================================================
 */
template <typename t_in, typename t_out>
class chan_readwrite_t : public CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		std::function<void(CSP::csp_chan<t_in,t_out>*, t_in&)>>
{
public:
	using funkname = std::function<void(CSP::csp_chan<t_in,t_out>*, t_in&)>;
	// This is the type we want to be, a CSP pipe
	using thistype = CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		funkname>;

	void run(funkname a)
	{
		t_in line;
		while (this->read(line))
			a(this, line);
	}
};
// csp_chan functor
template <typename t_in, typename t_out>
CSP::csp_chan
<
	t_in,
	t_out,
	CSP_CACHE_DEFAULT,
	std::function<void(t_in&)>
>
	chan_readwrite(
		std::function<void(CSP::csp_chan<t_in,t_out>*, t_in&)>
					asdf)
{
	using funkname = std::function<void(CSP::csp_chan<t_in,t_out>*, t_in&)>;
	// madotsuki_eating_soup.jpg
	using thistype = CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		funkname>;

	thistype result;

	result.arguments = std::make_tuple(asdf);
	result.start = (void(thistype::*)(funkname))
			&chan_readwrite_t<t_in,t_out>::run;

	return result;
} // chan_read


/* ================================================================
 * chan_select
 * Iterate over a pipe in-line, always output
 * cat(file) | chan_read<...>...
 * ================================================================
 */
template <typename t_in, typename t_out>
class chan_select_t : public CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		std::function<t_out(t_in&)>>
{
public:
	using funkname = std::function<t_out(t_in&)>;
	// This is the type we want to be, a CSP pipe
	using thistype = CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		funkname>;

	void run(funkname a)
	{
		t_in line;
		while (this->read(line))
			this->put(a(line));
	}
};
// csp_chan functor
template <typename t_in, typename t_out>
CSP::csp_chan
<
	t_in,
	t_out,
	CSP_CACHE_DEFAULT,
	std::function<t_out(t_in&)>
>
	chan_select(
		std::function<t_out(t_in&)>
					asdf)
{
	using funkname = std::function<t_out(t_in&)>;
	// madotsuki_eating_soup.jpg
	using thistype = CSP::csp_chan<
		t_in, t_out, CSP_CACHE_DEFAULT,
		funkname>;

	thistype result;

	result.arguments = std::make_tuple(asdf);
	result.start = (void(thistype::*)(funkname))
			&chan_select_t<t_in,t_out>::run;

	return result;
} // chan_read


/* ================================================================
 * chan_read
 * its chan_read without sending thisptr
 * (can't use thisptr->put() -- thus no output)
 * ================================================================
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
// csp_chan functor
template <typename t_in>
CSP::csp_chan
<
	t_in,
	CSP::nothing,
	CSP_CACHE_DEFAULT,
	std::function<void(t_in&)>
>
	chan_read(
		std::function<void(t_in&)>
					asdf)
{
	// madotsuki_eating_soup.jpg
	using funkname = std::function<void(t_in&)>;
	using thistype = CSP::csp_chan<
		t_in, CSP::nothing, CSP_CACHE_DEFAULT,
		funkname>;

	thistype result;

	result.arguments = std::make_tuple(asdf);
	result.start = (void (thistype::*)(funkname))
			&chan_sans_output_read_t<t_in>::run;

	return result;
} // chan_read

} // namespace CSP


#endif /* READ_H_ */
