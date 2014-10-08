/*
 * encap.cpp
 *
 *  Created on: Oct 5, 2014
 *      Author: skim
 */

#include <csp/csplib.h>
#include <csp/exec.h>

using namespace csp;

int main()
{
	auto chan = encap<string,string>(
			grab("cat", false) | grab("com", false) | grab("ing", false)
		);

	chan.put("excommunicating");
	chan.put("welcoming");
	chan.put("uncommunicative");
	chan.put("complicating");
	chan.put("compromised");
	chan.put("reading");
	chan.put("cat");
	chan.close_input();

	csp::string result;
	while (chan.read(result))
		std::cout << result << "\n";
}
