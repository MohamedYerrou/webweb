#include "../includes/CGIHandler.hpp"
#include "../includes/Client.hpp"
#include "../includes/Server.hpp"
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <string.h>
#include <vector>
#include <cstring>

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

std::string CGIHandler::getBuffer() const
{
    return buffer;
}

int CGIHandler::getOutFD() const
{
    return out_fd[0];
}


std::vector<std::string> Client::buildCGIEnv(const std::string& scriptPath)
{
    std::vector<std::string> env;
    
    env.push_back("GATEWAY_INTERFACE=CGI/1.0");
    env.push_back("SERVER_PROTOCOL=" + currentRequest->getProtocol());
    env.push_back("REQUEST_METHOD=" + currentRequest->getMethod());
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    // env.push_back("PATH_INFO=" + pathInfo);  // updated

    std::map<std::string, std::string> headers = currentRequest->getHeaders();
    if (headers.find("Host") != headers.end())
        env.push_back("HTTP_HOST=" + headers["Host"]);
    
    if (headers.find("Content-Type") != headers.end())
        env.push_back("CONTENT_TYPE=" + headers["Content-Type"]);

    if (headers.find("Content-Length") != headers.end())
        env.push_back("CONTENT_LENGTH=" + headers["Content-Length"]);

    env.push_back("SERVER_SOFTWARE=server/1.0");
    env.push_back("REDIRECT_STATUS=200");

    return env;
}



void Client::handleCGI()
{
    // Build environment
    std::vector<std::string> vecEnv = buildCGIEnv(newPath);
    std::map<std::string,std::string> envMap;
    for (std::size_t i = 0; i < vecEnv.size(); ++i)
    {
        std::size_t pos = vecEnv[i].find('=');
        if (pos != std::string::npos)
            envMap[vecEnv[i].substr(0, pos)] = vecEnv[i].substr(pos + 1);
    }

    // Start CGI if not already started
    if (!cgiHandler)
    {
        cgiHandler = new CGIHandler(this);
        try
        {
            cgiHandler->startCGI(newPath, envMap);
        }
        catch (const std::exception& e)
        {
            std::cerr << "[CGI] Exception while starting CGI: " << e.what() << std::endl;
            errorResponse(500, e.what());
            delete cgiHandler;
            cgiHandler = NULL;
            return;
        }
    }

    // Read output
    if (cgiHandler)
        cgiHandler->readOutput();

    // Send response if finished
    if (cgiHandler && cgiHandler->isFinished())
    {
        std::string body = cgiHandler->getBuffer();
        std::ostringstream oss;
        oss << body.size();

        std::string res = "HTTP/1.1 200 OK\r\n";
        res += "Content-Type: text/html\r\n";
        res += "Content-Length: " + oss.str() + "\r\n\r\n";
        res += body;

        send(getFD(), res.c_str(), res.size(), 0);
        setSentAll(true);

        delete cgiHandler;
        cgiHandler = NULL;
    }
}




void CGIHandler::startCGI(const std::string& scriptPath, const std::map<std::string,std::string>& env)
{
    
    int err_pipe[2];
    if (pipe(in_fd) == -1 || pipe(out_fd) == -1 || pipe(err_pipe) == -1) {
        int saved = errno;
        if (in_fd[0] != -1) { close(in_fd[0]); in_fd[0] = -1; }
        if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
        if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
        if (out_fd[1] != -1) { close(out_fd[1]); out_fd[1] = -1; }
        // if (err_pipe[0] != -1) { close(err_pipe[0]); err_pipe[0] = -1; }
        // if (err_pipe[1] != -1) { close(err_pipe[1]); err_pipe[1] = -1; }
        std::cerr << "[CGI] pipe() failed: " << strerror(saved) << "\n";
        throw std::runtime_error("pipe failed");
    }
    
    pid = fork();
    if (pid == -1) {
        int saved = errno;
        close(in_fd[0]); close(in_fd[1]); in_fd[0] = in_fd[1] = -1;
        close(out_fd[0]); close(out_fd[1]); out_fd[0] = out_fd[1] = -1;
        // close(err_pipe[0]); close(err_pipe[1]);
        std::cerr << "[CGI] fork() failed: " << strerror(saved) << "\n";
        throw std::runtime_error("fork failed");
    }
    std::cerr << "[CGI] startCGI: script=" << scriptPath
    << " pid=" << pid << " outfd=" << out_fd[0] << "\n";
    
    //TODO: Add HTTP Error response whenever something wrong happens in child (500)
    if (pid == 0)
    {
        close(in_fd[1]);
        close(out_fd[0]);
        // close(err_pipe[0]);

        dup2(in_fd[0], STDIN_FILENO);
        dup2(out_fd[1], STDOUT_FILENO);
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
            write(out_fd[1], "E", 1);
            for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
            exit(1);
        }

        char* argv[] = { const_cast<char*>(scriptPath.c_str()), NULL };
        execve(scriptPath.c_str(), argv, envp.empty() ? NULL : &envp[0]);

        write(out_fd[1], "E", 1);
        for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
        exit(1);
    }
    else
    {
        close(in_fd[0]);
        close(out_fd[1]);

        fcntl(out_fd[0], F_SETFL, O_NONBLOCK);
        fcntl(in_fd[1], F_SETFL, O_NONBLOCK);
        // close(err_pipe[1]);
        CGIContext ctx;

        ctx.childpid = pid;
        ctx.clientfd = client->getFD(); // TODO
        ctx.body = this->client->getBody();
        ctx.output_buffer = "";
        ctx.is_stdin_closed = 0;
        ctx.is_stdout_closed = 0;
        ctx.pipe_from_cgi = out_fd[0];
        ctx.pipe_to_cgi = in_fd[1];
        ctx.bytes_written = 0;
        ctx.is_error = 0;
        // char err = 0;
        // ssize_t n = read(err_pipe[0], &err, 1);
        // close(err_pipe[0]);
        // if (n == 1) {
        //     int status;
        //     waitpid(pid, &status, 0);
        //     pid = -1;
        //     if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
        //     if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
        //     std::cerr << "[CGI] execve() failed in child for " << scriptPath << "\n";
        //     throw std::runtime_error(std::string("execve failed for ") + scriptPath);
        // }
         
        started = true;
    }
}



void CGIHandler::readOutput()
{
	// 1. Basic checks
	if (!started || finished || out_fd[0] == -1)
		return;

	char buf[1024];
	
	// 2. Perform a single read operation.
	// We rely completely on the caller (the main loop) to only call this
	// when poll() has indicated POLLIN, making an immediate -1 return less likely
	// to be a transient EAGAIN (which we cannot check for anyway).
	ssize_t n = read(out_fd[0], buf, sizeof(buf));

	if (n > 0)
	{
		// Data successfully read. Append it.
		buffer.append(buf, n);
	}
	else if (n == 0)
	{
		// End-of-file (CGI process has closed the pipe).
		// This is the normal completion signal.
		finished = true;
		
		// Attempt to reap the child process gracefully (non-blocking wait)
		if (pid > 0) {
			waitpid(pid, NULL, WNOHANG);
			// We don't set pid = -1 here yet, as we might need to check its status later 
			// in the handler (though cleaning up in the destructor is safer).
		}
		
		// Close the read end of the pipe
		close(out_fd[0]);
		out_fd[0] = -1;
	}
	else // n == -1 (Error)
	{
		// Since checking errno is strictly forbidden, any -1 is treated as a fatal error.
		// This covers all persistent errors, including potential (but rare) errors 
		// even after poll() indicated readiness, and is the safest approach.
		
		finished = true;
		
		// Attempt to reap the child process (non-blocking wait)
		if (pid > 0) {
			waitpid(pid, NULL, WNOHANG);
		}
		
		// Close the pipe and terminate the process gracefully.
		close(out_fd[0]);
		out_fd[0] = -1;
	}
}

void Client::checkCGIValid()
{
    // 1. Check Request Method Authorization (Nginx uses 405 Method Not Allowed)
    const std::vector<std::string>& allowedMethods = location->getMethod();
    const std::string& requestMethod = currentRequest->getMethod();
    bool methodAllowed = false;

    for (size_t i = 0; i < allowedMethods.size(); ++i)
    {
        if (allowedMethods[i] == requestMethod)
        {
            methodAllowed = true;
            break;
        }
    }

    if (!methodAllowed)
    {
        std::cerr << "[CGI Check] Method " << requestMethod << " not allowed for path: " << currentRequest->getPath() << std::endl;
        return errorResponse(405, "Method Not Allowed");
    }

    // 2. Determine if CGI processing is configured at all for this location
    const std::map<std::string, std::string>& cgiMap = location->getCgi();
    bool cgiConfigured = !cgiMap.empty();

    // 3. Handle Path Resolution: Directory vs. File

    if (isDir(newPath))
    {
        // Path points to a directory. Look for index files (Nginx-like behavior).
        const std::vector<std::string>& indexFiles = location->getIndex();
        bool indexFound = false;
        std::string originalPath = newPath;

        if (!indexFiles.empty())
        {
            // Ensure directory path ends with a slash for proper index file joining
            if (originalPath.length() > 0 && originalPath[originalPath.length() - 1] != '/')
            {
                originalPath += "/";
            }

            for (size_t i = 0; i < indexFiles.size(); ++i)
            {
                std::string indexPath = originalPath + indexFiles[i];

                if (isFile(indexPath))
                {
                    newPath = indexPath; // Update newPath to the actual index file path
                    indexFound = true;
                    break;
                }
            }
        }

        if (!indexFound)
        {
            if (location->getAutoIndex())
            {
                std::cout << "[CGI Check] Listing directory (Autoindex ON): " << newPath << std::endl;
                return listingDirectory(newPath);
            }
            else
            {
                std::cerr << "[CGI Check] Index file not found and autoindex is OFF: " << newPath << std::endl;
                // Nginx returns 403 Forbidden when an index is missing and autoindex is off.
                return errorResponse(403, "Forbidden: Directory listing denied");
            }
        }
    }
    else if (!isFile(newPath))
    {
        // Path is neither a directory nor a file.
        std::cerr << "[CGI Check] Resource not found: " << newPath << std::endl;
        return errorResponse(404, "Not Found");
    }

    // 4. File Handling (Reached here if newPath is confirmed to be a regular file)

    if (cgiConfigured)
    {
        std::size_t dotPos = newPath.find_last_of('.');

        if (dotPos != std::string::npos)
        {
            std::string ext = newPath.substr(dotPos);
            std::map<std::string, std::string>::const_iterator it = cgiMap.find(ext);

            if (it != cgiMap.end())
            {
                // File extension matches a configured CGI interpreter
                
                // CRITICAL NGINX-LIKE CHECK: Ensure the file is executable (X_OK).
                // Prevents execve failure loop and returns 403, not 500.
                if (access(newPath.c_str(), X_OK) == 0)
                {
                    setIsCGI(true);
                    std::cout << "[CGI Check] Serving CGI script with interpreter: " << it->second << " for path: " << newPath << std::endl;
                    return; // CGI processing flagged.
                }
                else
                {
                    setIsCGI(false);
                    std::cerr << "[CGI Check] Matched CGI extension '" << ext << "' but script is not executable (403): " << newPath << std::endl;
                    return errorResponse(403, "Forbidden: CGI script is not executable or invalid.");
                }
            }
        }
    }

    // 5. Default to Static File (Final Fallback)
    // If we reach this point, the resource is a file, but it did not match any CGI rules.
    setIsCGI(false);
    std::cout << "[CGI Check] Serving static file: " << newPath << std::endl;

}
