#include <csp/csplib.h>
#include <csp/exec.h>

using namespace csp;

// Test exec functions
int main()
{
	std::atomic<int> error1 (0), error2 (0), error3 (0);

	exec_r("cat /usr/share/dict/words | grep cat", &error1) |
			exec_rw("grep com", &error2) |
			exec_w("grep ing", &error3);

	return error1 | error2 | error3;
}
