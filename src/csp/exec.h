/*
 * exec.h
 *
 *  Created on: Sep 26, 2014
 *      Author: skim
 */

#ifndef EXEC_H_
#define EXEC_H_

#include <atomic>
#include <stdio.h>
#include <csp/pipe.h>

namespace csp{

// Reads FILE into message_stream
void read_file_to(FILE* fp, message_stream<csp::string>* writeto)
{
	size_t len = 0;
	ssize_t amt;
	char* line = NULL;
	while ((amt = getline(&line, &len, fp)) != -1)
	{
		if (!amt) continue;
		if (line[amt - 1] == '\n')
			amt--;
		writeto->write(csp::string (line, amt));
	}
}
// Reads from message_stream into FILE
void write_file_from(FILE* fp, message_stream<csp::string>* readfrom)
{
	csp::string in;
	while (readfrom->read(in))
	{
		in.push_back('\n');
		if (fwrite(&in[0], 1, in.size(), fp) < in.size())
			// Error
			return;
	}
}

// Run a program and read its output
CSP_DECL(exec_r, csp::nothing, csp::string,
		const char*, std::atomic<int>*)
(const char* cmd, std::atomic<int>* error)
{
	// Open command for reading
	FILE* fp = popen(cmd, "r");
	if (!fp)
	{
		*error = 1;
		return;
	}

	read_file_to(fp, this->csp_output);
	if (pclose(fp))
		*error = 2;
}

// Run a program and write to its input
CSP_DECL(exec_w, csp::string, csp::nothing,
		const char*, std::atomic<int>*)
(const char* cmd, std::atomic<int>* error)
{
	// Open command for writing
	FILE* fp = popen(cmd, "w");
	if (!fp)
	{
		*error = 1;
		return;
	}

	write_file_from(fp, this->csp_input);
	if (pclose(fp))
		*error = 2;
}

// http://dzone.com/snippets/simple-popen2-implementation
// Bi-directional popen
int bipopen(const char *command, int *infp, int *outfp)
{
	int p_stdin[2], p_stdout[2];

	if (pipe(p_stdin) || pipe(p_stdout))
		return 1;

	auto pid = fork();

	if (pid == -1)
		return 2;
	else if (pid == 0)
	{
		close(p_stdin[STDOUT_FILENO]);
		dup2(p_stdin[STDIN_FILENO], STDIN_FILENO);
		close(p_stdout[STDIN_FILENO]);
		dup2(p_stdout[STDOUT_FILENO], STDOUT_FILENO);

		execl("/bin/sh", "/bin/sh", "-c", command, NULL);
		exit(1);
	}

	close(p_stdin[STDIN_FILENO]);
	close(p_stdout[STDOUT_FILENO]);
	*infp = p_stdin[STDOUT_FILENO];
	*outfp = p_stdout[STDIN_FILENO];

	return 0;
}

void _bg_read(int fd, message_stream<csp::string>* stream)
{
	FILE* fp = fdopen(fd, "r");
	read_file_to(fp, stream);
	close(fd);
}
// Run a program and read from and write to it
// cat(something) | exec_rw("grep stuff") | print();
CSP_DECL(exec_rw, csp::string, csp::string,
		const char*, std::atomic<int>*)
(const char* cmd, std::atomic<int>* error)
{
	int infd, outfd;
	if ((*error = bipopen(cmd, &infd, &outfd)))
		return;
	FILE* infp = fdopen(infd, "w");

	if (!infp)
	{
		*error = 3;
		close(infd);
		close(outfd);
		return;
	}

	auto thread = std::thread(_bg_read, outfd, csp_output);
	write_file_from(infp, csp_input);
	close(infd);
	thread.join();
}

} /* namespace csp */

#endif /* EXEC_H_ */
