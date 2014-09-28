#include <csp/csplib.h>
#include <csp/parallel.h>

using namespace csp;

CSP_DECL(sleepsort, nothing, string, int)(int in)
{
	sleep(in);
	put(std::to_string(in));
}

int main()
{
	std::vector<int> nums {3, 5, 1, 2, 4, 0};
	vec(nums) | schedule(sleepsort) | print();
	return 0;
}
