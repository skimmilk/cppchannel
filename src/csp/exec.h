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
#include <sys/wait.h>

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
	if (line)
		free(line);
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

// All open file descriptors need to be closed in the child process
void close_files()
{
	// Close all that isn't STDIN or STDOUT
	int maxfd=sysconf(_SC_OPEN_MAX);
	for(int fd=3; fd<maxfd; fd++)
		close(fd);
}
// Closes all open file descriptors and fires command
// Returns error
int fire_and_forget(const char* cmd, const char* flags, FILE** fp, pid_t& pid)
{
	int pipes[2];
	if (pipe(pipes))
		return 1;

	pid = fork();
	if (pid == -1)
		return 2;

	// Child
	if (!pid)
	{
		if (*flags == 'r')
		{
			close(pipes[0]);
			if (pipes[1] != STDOUT_FILENO)
			{
				dup2(pipes[1], STDOUT_FILENO);
				close(pipes[1]);
			}
		}
		else
		{
			close(pipes[1]);
			if (pipes[0] != STDIN_FILENO)
			{
				dup2(pipes[0], STDIN_FILENO);
				close(pipes[0]);
			}
		}
		close_files();
		execl("/bin/sh", "/bin/sh", "-c", cmd, NULL);
		perror("Could not run command");
	}
	// Parent
	if (*flags == 'r')
	{
		*fp = fdopen(pipes[0], flags);
		close(pipes[1]);
	}
	else
	{
		*fp = fdopen(pipes[1], flags);
		close(pipes[0]);
	}

	return 0;
}
// Run a program and read its output
CSP_DECL(exec_r, csp::nothing, csp::string,
		const char*, std::atomic<int>*)
(const char* cmd, std::atomic<int>* error)
{
	// Open command for reading
	FILE* fp;
	pid_t pid;
	if ((*error = fire_and_forget(cmd, "r", &fp, pid)) || !fp)
		return;

	read_file_to(fp, this->csp_output);
	if (pclose(fp))
		*error = 2;
	waitpid(pid, NULL, 0);
}

// Run a program and write to its input
CSP_DECL(exec_w, csp::string, csp::nothing,
		const char*, std::atomic<int>*)
(const char* cmd, std::atomic<int>* error)
{
	// Open command for writing
	FILE* fp;
	pid_t pid;
	if ((*error = fire_and_forget(cmd, "w", &fp, pid)) || !fp)
		return;

	write_file_from(fp, this->csp_input);
	if (pclose(fp))
		*error = 2;
	waitpid(pid, NULL, 0);
}

// http://dzone.com/snippets/simple-popen2-implementation
// Bi-directional popen
int bipopen(const char *command, int *infp, int *outfp, pid_t& pid)
{
	int p_stdin[2], p_stdout[2];

	if (pipe(p_stdin) || pipe(p_stdout))
		return 1;

	pid = fork();

	if (pid == -1)
		return 2;
	else if (pid == 0)
	{
		close(p_stdin[STDOUT_FILENO]);
		dup2(p_stdin[STDIN_FILENO], STDIN_FILENO);
		close(p_stdout[STDIN_FILENO]);
		dup2(p_stdout[STDOUT_FILENO], STDOUT_FILENO);

		close_files();

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
	pclose(fp);
}
// Run a program and read from and write to it
// cat(something) | exec_rw("grep stuff") | print();
CSP_DECL(exec_rw, csp::string, csp::string,
		const char*, std::atomic<int>*)
(const char* cmd, std::atomic<int>* error)
{
	int infd, outfd;
	pid_t pid;
	if ((*error = bipopen(cmd, &infd, &outfd, pid)))
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
	pclose(infp);
	thread.join();
	waitpid(pid, NULL, 0);
}

} /* namespace csp */

#endif /* EXEC_H_ */
