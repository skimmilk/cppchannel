/*
 * read.h
 *
 *  Created on: Apr 16, 2014
 *      Author: skim
 */

#ifndef READ_H_
#define READ_H_

#include <csp/csplib.h>

namespace csp{

/* ================================================================
 * chan_select
 * Only output input when the overloaded function returns true
 * ================================================================
 */
template <typename t_in>
class chan_select_t : public csp::channel<
		t_in, t_in,
		std::function<bool(t_in&)>>
{
public:
	using funkname = std::function<bool(t_in&)>;
	// This is the type we want to be, a csp pipe
	using thistype = csp::channel<
		t_in, t_in,
		funkname>;

	void run(funkname a)
	{
		t_in line;
		while (this->read(line))
			if(a(line))	this->put(line);
	}
};
// channel functor
template <typename t_in>
csp::shared_ptr<csp::channel
<
	t_in,
	t_in,
	std::function<bool(t_in&)>
>>
	chan_select(
		std::function<bool(t_in&)>
					asdf)
{
	using funkname = std::function<bool(t_in&)>;
	using thistype = csp::channel<
		t_in, t_in,
		funkname>;
	auto result = csp::make_shared<thistype>();

	result->arguments = std::make_tuple(asdf);
	result->start = (void(thistype::*)(funkname))
			&chan_select_t<t_in>::run;

	return result;
} // chan_select

/* ================================================================
 * chan_readwrite
 * Iterate over a pipe in-line, selectively output
 * cat(file) | chan_read<...>...
 * ================================================================
 */
template <typename t_in, typename t_out>
class chan_readwrite_t : public csp::channel<
		t_in, t_out,
		std::function<void(csp::channel<t_in,t_out>*, t_in&)>>
{
public:
	using funkname = std::function<void(csp::channel<t_in,t_out>*, t_in&)>;
	// This is the type we want to be, a csp pipe
	using thistype = csp::channel<
		t_in, t_out,
		funkname>;

	void run(funkname a)
	{
		t_in line;
		while (this->read(line))
			a(this, line);
	}
};
// channel functor
template <typename t_in, typename t_out>
csp::shared_ptr<csp::channel
<
	t_in,
	t_out,
	std::function<void(t_in&)>
>>
	chan_readwrite(
		std::function<void(csp::channel<t_in,t_out>*, t_in&)>
					asdf)
{
	using funkname = std::function<void(csp::channel<t_in,t_out>*, t_in&)>;
	// madotsuki_eating_soup.jpg
	using thistype = csp::channel<
		t_in, t_out,
		funkname>;

	auto result = csp::make_shared<thistype>();

	result->arguments = std::make_tuple(asdf);
	result->start = (void(thistype::*)(funkname))
			&chan_readwrite_t<t_in,t_out>::run;

	return result;
} // chan_read


/* ================================================================
 * chan_iter
 * Iterate over a pipe in-line, always output
 * cat(file) | chan_read<...>...
 * ================================================================
 */
template <typename t_in, typename t_out>
class chan_iter_t : public csp::channel<
		t_in, t_out,
		std::function<t_out(t_in&)>>
{
public:
	using funkname = std::function<t_out(t_in&)>;
	// This is the type we want to be, a csp pipe
	using thistype = csp::channel<
		t_in, t_out,
		funkname>;

	void run(funkname a)
	{
		t_in line;
		while (this->read(line))
			this->put(a(line));
	}
};
// channel functor
template <typename t_in, typename t_out>
csp::shared_ptr<csp::channel
<
	t_in,
	t_out,
	std::function<t_out(t_in&)>
>>
	chan_iter(
		std::function<t_out(t_in&)>
					asdf)
{
	using funkname = std::function<t_out(t_in&)>;
	// madotsuki_eating_soup.jpg
	using thistype = csp::channel<
		t_in, t_out,
		funkname>;

	auto result = csp::make_shared<thistype>();

	result->arguments = std::make_tuple(asdf);
	result->start = (void(thistype::*)(funkname))
			&chan_iter_t<t_in,t_out>::run;

	return result;
} // chan_read


/* ================================================================
 * chan_read
 * Non-outputting read
 * its chan_read without sending thisptr
 * (can't use thisptr->put() -- thus no output)
 * ================================================================
 */
template <typename t_in>
class chan_sans_output_read_t : public csp::channel<
		t_in, csp::nothing,
		std::function<void(t_in&)>>
{
public:
	// This is the type we want to be, a csp pipe
	using thistype = csp::channel<
		t_in, csp::nothing,
		std::function<void(t_in&)>>;

	void run(std::function<void(t_in&)> a)
	{
		t_in line;
		while (this->read(line))
			a(line);
	}
};
// channel functor
template <typename t_in>
csp::shared_ptr<csp::channel
<
	t_in,
	csp::nothing,
	std::function<void(t_in&)>
>>
	chan_read(
		std::function<void(t_in&)>
					asdf)
{
	// madotsuki_eating_soup.jpg
	using funkname = std::function<void(t_in&)>;
	using thistype = csp::channel<
		t_in, csp::nothing,
		funkname>;

	auto result = csp::make_shared<thistype>();

	result->arguments = std::make_tuple(asdf);
	result->start = (void (thistype::*)(funkname))
			&chan_sans_output_read_t<t_in>::run;

	return result;
} // chan_read

} // namespace csp


#endif /* READ_H_ */
