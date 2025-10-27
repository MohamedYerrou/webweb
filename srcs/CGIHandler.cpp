#include "../includes/CGIHandler.hpp"
#include "../includes/Client.hpp"
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <vector>
#include <cstring>
#include <errno.h> // For errno

CGIHandler::CGIHandler(Client* c)
: client(c), pid(-1), started(false), finished(false)
{
    (void)client;
    in_fd[0] = in_fd[1] = -1;
    out_fd[0] = out_fd[1] = -1;
}

CGIHandler::~CGIHandler()
{
    if (in_fd[0] != -1) { close(in_fd[0]); in_fd[0] = -1; }
    if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
    if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
    if (out_fd[1] != -1) { close(out_fd[1]); out_fd[1] = -1; }

    if (pid > 0) {
    int status;
    if (waitpid(pid, &status, WNOHANG) <= 0) { 
        kill(pid, SIGKILL); 
        waitpid(pid, &status, 0);
        }
    }
}


bool CGIHandler::isFinished() const
{
    return finished;
}

// Added
void CGIHandler::setFinished(bool val)
{
    finished = val;
}

std::string CGIHandler::getBuffer() const
{
    return buffer;
}

// Added
void CGIHandler::clearBuffer()
{
	buffer.clear();
}

int CGIHandler::getOutFD() const
{
    return out_fd[0];
}

// Added
int CGIHandler::getInFD() const
{
    return in_fd[1];
}

// Added
void CGIHandler::closeInFD()
{
    if (in_fd[1] != -1)
    {
        close(in_fd[1]);
        in_fd[1] = -1;
    }
}


//
// Client::buildCGIEnv() and Client::handleCGI() were REMOVED from this file.
// They belong in Client.cpp
//


void CGIHandler::startCGI(const std::string& scriptPath, const std::map<std::string,std::string>& env)
{
    
    int err_pipe[2];
    if (pipe(in_fd) == -1 || pipe(out_fd) == -1 || pipe(err_pipe) == -1) {
        int saved = errno;
        if (in_fd[0] != -1) { close(in_fd[0]); in_fd[0] = -1; }
        if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
        if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
        if (out_fd[1] != -1) { close(out_fd[1]); out_fd[1] = -1; }
        if (err_pipe[0] != -1) { close(err_pipe[0]); err_pipe[0] = -1; }
        if (err_pipe[1] != -1) { close(err_pipe[1]); err_pipe[1] = -1; }
        std::cerr << "[CGI] pipe() failed: " << strerror(saved) << "\n";
        throw std::runtime_error("pipe failed");
    }
    
    pid = fork();
    if (pid == -1) {
        int saved = errno;
        close(in_fd[0]); close(in_fd[1]); in_fd[0] = in_fd[1] = -1;
        close(out_fd[0]); close(out_fd[1]); out_fd[0] = out_fd[1] = -1;
        close(err_pipe[0]); close(err_pipe[1]);
        std::cerr << "[CGI] fork() failed: " << strerror(saved) << "\n";
        throw std::runtime_error("fork failed");
    }

    //
    // The std::cerr log was here. It's been moved to the parent (else) block.
    //
    
    if (pid == 0)
    {
        // --- Child Process ---
        close(in_fd[1]); // Close write end of parent-to-child pipe
        close(out_fd[0]); // Close read end of child-to-parent pipe
        close(err_pipe[0]); // Close read end of error pipe

        dup2(in_fd[0], STDIN_FILENO);
        dup2(out_fd[1], STDOUT_FILENO);
        dup2(out_fd[1], STDERR_FILENO); // Redirect stderr to stdout as well
        close(in_fd[0]);
        close(out_fd[1]);

        std::vector<char*> envp;
        for (std::map<std::string,std::string>::const_iterator it = env.begin(); it != env.end(); ++it)
        {
            std::string s = it->first + "=" + it->second;
            char* cstr = new char[s.size() + 1];
            strcpy(cstr, s.c_str());
            envp.push_back(cstr);
        }
        envp.push_back(NULL);
        
        if (access(scriptPath.c_str(), X_OK) != 0) {
            write(err_pipe[1], "E", 1);
            for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
            exit(1);
        }

        // --- Modified Execve to handle interpreter ---
        std::map<std::string, std::string>::const_iterator it = env.find("CGI_INTERPRETER");
        std::string interpreter = (it != env.end()) ? it->second : "";

        char** argv;
        if (!interpreter.empty() && interpreter != "none")
        {
            // Using an interpreter (e.g., /usr/bin/python)
            argv = new char*[3];
            
            // --- FIX ---
            // argv[0] is conventionally the *name* of the program, not the full path.
            std::string interpName = interpreter;
            size_t slashPos = interpreter.rfind('/');
            if (slashPos != std::string::npos) {
                interpName = interpreter.substr(slashPos + 1);
            }
            argv[0] = const_cast<char*>(interpName.c_str()); // e.g., "python3"
            // --- END FIX ---

            argv[1] = const_cast<char*>(scriptPath.c_str()); // e.g., "/path/to/script.py"
            argv[2] = NULL;
            execve(interpreter.c_str(), argv, envp.empty() ? NULL : &envp[0]);
        }
        else
        {
            // Executing directly
            argv = new char*[2];
            argv[0] = const_cast<char*>(scriptPath.c_str());
            argv[1] = NULL;
            execve(scriptPath.c_str(), argv, envp.empty() ? NULL : &envp[0]);
        }
        // --- End of modification ---


        write(err_pipe[1], "E", 1);
        for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
		delete[] argv; // Clean up argv
        exit(1);
    }
    else
    {
        // --- Parent Process ---

        // --- FIX ---
        // Moved the log message here. Only the parent will print it.
        std::cerr << "[CGI] startCGI: script=" << scriptPath
        << " pid=" << pid << " outfd=" << out_fd[0] << " infd=" << in_fd[1] << "\n";
        // --- END FIX ---

        close(in_fd[0]); in_fd[0] = -1;
        close(out_fd[1]); out_fd[1] = -1;
        close(err_pipe[1]);

        char err = 0;
        ssize_t n = read(err_pipe[0], &err, 1);
        close(err_pipe[0]);
        if (n == 1) {
            int status;
            waitpid(pid, &status, 0);
            pid = -1;
            if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
            if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
            std::cerr << "[CGI] execve() failed in child for " << scriptPath << "\n";
            throw std::runtime_error(std::string("execve failed for ") + scriptPath);
        }

        // Set *both* pipes to non-blocking
        fcntl(out_fd[0], F_SETFL, O_NONBLOCK);
        fcntl(in_fd[1], F_SETFL, O_NONBLOCK); // Added
        started = true;
    }
}


// --- Rewritten to be non-blocking ---
void CGIHandler::readOutput()
{
	// 1. Basic checks
	if (!started || finished || out_fd[0] == -1)
		return;

	char buf[4096]; // Larger buffer
	
	// 2. Read in a loop until it's empty or would block
	while (true)
	{
		ssize_t n = read(out_fd[0], buf, sizeof(buf));

		if (n > 0)
		{
			// Data successfully read. Append it to internal buffer.
			buffer.append(buf, n);
		}
		else if (n == 0)
		{
			// End-of-file (CGI process has closed the pipe).
			finished = true;
			if (pid > 0) {
				waitpid(pid, NULL, WNOHANG);
			}
			close(out_fd[0]);
			out_fd[0] = -1;
			break; // Stop reading
		}
		else // n == -1 (Error)
		{
            // IMPORTANT: With non-blocking I/O, we *must* check errno
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				// No more data to read right now
				break; // Stop reading
			}
			else
			{
				// A real read error
				finished = true;
				if (pid > 0) {
					waitpid(pid, NULL, WNOHANG);
				}
				close(out_fd[0]);
				out_fd[0] = -1;
                std::cerr << "[CGI] read() error: " << strerror(errno) << "\n";
				break; // Stop reading
			}
		}
	}
}

//
// Client::checkCGIValid() was REMOVED from this file
//