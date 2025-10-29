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
: client(c), pid(-1), started(false), finished(false), bytes_written(0), _error(false)
{
    (void)client;
    in_fd[0] = in_fd[1] = -1;
    out_fd[0] = out_fd[1] = -1;
}

CGIHandler::~CGIHandler()
{
    if (in_fd[0] != -1) { 
        close(in_fd[0]); in_fd[0] = -1; 
    }
    if (in_fd[1] != -1) { 
        close(in_fd[1]); in_fd[1] = -1; 
    }
    if (out_fd[0] != -1) { 
        close(out_fd[0]); out_fd[0] = -1; 
    }
    if (out_fd[1] != -1) { 
        close(out_fd[1]); out_fd[1] = -1; 
    }

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
    
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=" + currentRequest->getProtocol());
    env.push_back("REQUEST_METHOD=" + currentRequest->getMethod());
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    env.push_back("SCRIPT_NAME=" + currentRequest->getPath());
    
    std::string uri = currentRequest->getPath();
    size_t qPos = uri.find('?');
    if (qPos != std::string::npos)
    {
        env.push_back("QUERY_STRING=" + uri.substr(qPos + 1));
        env.push_back("PATH_INFO=" + uri.substr(0, qPos));
    }
    else
    {
        env.push_back("QUERY_STRING=");
        env.push_back("PATH_INFO=");
    }
    
    env.push_back("PATH_TRANSLATED=" + scriptPath);
    
    env.push_back("SERVER_SOFTWARE=webserv/1.0");
    std::map<std::string, std::string> headers = currentRequest->getHeaders();
    
    if (headers.find("Host") != headers.end())
    {
        std::string host = headers["Host"];
        size_t colonPos = host.find(':');
        if (colonPos != std::string::npos)
        {
            env.push_back("SERVER_NAME=" + host.substr(0, colonPos));
            env.push_back("SERVER_PORT=" + host.substr(colonPos + 1));
        }
        else
        {
            env.push_back("SERVER_NAME=" + host);
            env.push_back("SERVER_PORT=80");
        }
        env.push_back("HTTP_HOST=" + host);
    }
    else
    {
        env.push_back("SERVER_NAME=localhost");
        env.push_back("SERVER_PORT=80");
    }
    
    if (headers.find("Content-Type") != headers.end()) {
        env.push_back("CONTENT_TYPE=" + headers["Content-Type"]);
    } else {
        env.push_back("CONTENT_TYPE=");
    }
    
    if (headers.find("Content-Length") != headers.end()) {
        env.push_back("CONTENT_LENGTH=" + headers["Content-Length"]);
    } else {
        env.push_back("CONTENT_LENGTH=0");
    }
    
    env.push_back("REMOTE_ADDR=127.0.0.1");
    env.push_back("REMOTE_HOST=127.0.0.1");
    
    if (headers.find("User-Agent") != headers.end())
        env.push_back("HTTP_USER_AGENT=" + headers["User-Agent"]);
    if (headers.find("Accept") != headers.end())
        env.push_back("HTTP_ACCEPT=" + headers["Accept"]);
    if (headers.find("Accept-Language") != headers.end())
        env.push_back("HTTP_ACCEPT_LANGUAGE=" + headers["Accept-Language"]);
    if (headers.find("Accept-Encoding") != headers.end())
        env.push_back("HTTP_ACCEPT_ENCODING=" + headers["Accept-Encoding"]);
    if (headers.find("Cookie") != headers.end())
        env.push_back("HTTP_COOKIE=" + headers["Cookie"]);
    if (headers.find("Referer") != headers.end())
        env.push_back("HTTP_REFERER=" + headers["Referer"]);
    
    env.push_back("REDIRECT_STATUS=200");
    
    return env;
}

#include <sstream>

void Client::handleCGI()
{
    if (!cgiHandler)
    {
        std::vector<std::string> vecEnv = buildCGIEnv(newPath);
        std::map<std::string,std::string> envMap;
        for (std::size_t i = 0; i < vecEnv.size(); ++i)
        {
            std::size_t pos = vecEnv[i].find('=');
            if (pos != std::string::npos)
                envMap[vecEnv[i].substr(0, pos)] = vecEnv[i].substr(pos + 1);
        }

        cgiHandler = new CGIHandler(this);
        
        try
        {
            cgiHandler->startCGI(newPath, envMap);
            return;
        }
        catch (const std::exception& e)
        {
            errorResponse(500, e.what());
            delete cgiHandler;
            cgiHandler = NULL;
            return;
        }
    }
    
    if (cgiHandler && cgiHandler->isFinished())
    {
        std::string rawOutput = cgiHandler->getBuffer();

        std::string responseBody;
        std::string responseHeaders;

        size_t headerEndPos = rawOutput.find("\r\n\r\n");
        if (headerEndPos == std::string::npos)
        {
            headerEndPos = rawOutput.find("\n\n");
        }

        if (headerEndPos == std::string::npos)
        {
            responseBody = rawOutput;
            
            std::ostringstream oss;
            oss << responseBody.size();
            
            responseHeaders = "HTTP/1.1 200 OK\r\n";
            responseHeaders += "Content-Type: text/html\r\n";
            responseHeaders += "Content-Length: " + oss.str() + "\r\n";
        }
        else
        {
            responseBody = rawOutput.substr(headerEndPos + (rawOutput[headerEndPos] == '\r' ? 4 : 2));
            
            std::string cgiHeaderPart = rawOutput.substr(0, headerEndPos);
            
            std::ostringstream ossBodySize;
            ossBodySize << responseBody.size();

            responseHeaders = "HTTP/1.1 ";
            
            size_t statusPos = cgiHeaderPart.find("Status: ");
            if (statusPos != std::string::npos)
            {
                size_t statusEnd = cgiHeaderPart.find("\r\n", statusPos);
                if (statusEnd == std::string::npos) statusEnd = cgiHeaderPart.find('\n', statusPos);
                if (statusEnd == std::string::npos) statusEnd = cgiHeaderPart.length();

                responseHeaders += cgiHeaderPart.substr(statusPos + 8, statusEnd - (statusPos + 8)) + "\r\n";
            }
            else
            {
                responseHeaders += "200 OK\r\n";
            }
            
            responseHeaders += cgiHeaderPart + "\r\n";
            responseHeaders += "Content-Length: " + ossBodySize.str() + "\r\n";
        }
        
        std::string finalResponse = responseHeaders + "\r\n" + responseBody;

        send(getFD(), finalResponse.c_str(), finalResponse.size(), 0);
        
        setSentAll(true);

        delete cgiHandler;
        cgiHandler = NULL;
    }
}

void CGIHandler::startCGI(const std::string& scriptPath, const std::map<std::string,std::string>& env)
{
    int err_pipe[2];
    if (pipe(in_fd) == -1 || pipe(out_fd) == -1 || pipe(err_pipe) == -1) {
        if (in_fd[0] != -1) { close(in_fd[0]); in_fd[0] = -1; }
        if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
        if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
        if (out_fd[1] != -1) { close(out_fd[1]); out_fd[1] = -1; }
        if (err_pipe[0] != -1) { close(err_pipe[0]); err_pipe[0] = -1; }
        if (err_pipe[1] != -1) { close(err_pipe[1]); err_pipe[1] = -1; }
        throw std::runtime_error("pipe failed");
    }
    
    pid = fork();
    
    if (pid == -1) {
        close(in_fd[0]); close(in_fd[1]); in_fd[0] = in_fd[1] = -1;
        close(out_fd[0]); close(out_fd[1]); out_fd[0] = out_fd[1] = -1;
        close(err_pipe[0]); close(err_pipe[1]);
        throw std::runtime_error("fork failed");
    }
    
    if (pid == 0)
    {
        close(in_fd[1]);
        close(out_fd[0]);
        close(err_pipe[0]);

        dup2(in_fd[0], STDIN_FILENO);
        dup2(out_fd[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);

        close(in_fd[0]);
        close(out_fd[1]);
        close(err_pipe[1]);
        
        std::vector<char*> envp;
        for (std::map<std::string,std::string>::const_iterator it = env.begin(); it != env.end(); ++it)
        {
            std::string s = it->first + "=" + it->second;
            char* cstr = new char[s.size() + 1];
            strcpy(cstr, s.c_str());
            envp.push_back(cstr);
        }
        envp.push_back(NULL);
        
        if (access(scriptPath.c_str(), F_OK) != 0) {
            write(err_pipe[1], "E", 1);
            for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
            exit(1);
        }
        
        if (access(scriptPath.c_str(), X_OK) != 0) {
            write(err_pipe[1], "E", 1);
            for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
            exit(1);
        }

        char* argv[] = { const_cast<char*>(scriptPath.c_str()), NULL };
        execve(scriptPath.c_str(), argv, envp.empty() ? NULL : &envp[0]);

        write(err_pipe[1], "E", 1);
        for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
        exit(1);
    }
    else
    {
        close(in_fd[0]);
        close(out_fd[1]);

        fcntl(out_fd[0], F_SETFL, O_NONBLOCK);
        fcntl(in_fd[1], F_SETFL, O_NONBLOCK);
        
        close(err_pipe[1]);
        
        CGIContext ctx;
        ctx.childpid = pid;
        ctx.clientfd = client->getFD();
        ctx.body = this->client->getBody();
        ctx.output_buffer = "";
        ctx.is_stdin_closed = 0;
        ctx.is_stdout_closed = 0;
        ctx.pipe_from_cgi = out_fd[0];
        ctx.pipe_to_cgi = in_fd[1];
        ctx.bytes_written = 0;
        ctx.is_error = 0;
        
        started = true;
    }
}

void CGIHandler::readOutput()
{
    if (!started || finished || out_fd[0] == -1)
    {
        return;
    }

    char buf[1024];
    
    ssize_t n = read(out_fd[0], buf, sizeof(buf));

    if (n > 0)
    {
        buffer.append(buf, n);
    }
    else if (n == 0)
    {
        finished = true;
        
        if (pid > 0)
        {
            int status;
            waitpid(pid, &status, WNOHANG);
        }
        
        close(out_fd[0]);
        out_fd[0] = -1;
    }
    else
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            finished = true;
            if (pid > 0)
                waitpid(pid, NULL, WNOHANG);
            close(out_fd[0]);
            out_fd[0] = -1;
        }
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
                originalPath += "/";

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

void CGIHandler::setError(bool error)
{
    finished = true;
    _error = error;
}

void CGIHandler::setComplete(bool complete)
{
    finished = complete;
    if (complete)
        _error = false;
}

bool CGIHandler::is_Error() const
{
    return _error;
}

size_t CGIHandler::getBytesWritten() const
{
    return bytes_written;
}

void CGIHandler::addBytesWritten(size_t bytes)
{
    bytes_written += bytes;
}

void CGIHandler::appendResponse(const char* buf, ssize_t length)
{
    if (length > 0)
    {
        buffer.append(buf, length);
    }
}

int CGIHandler::getPid() const
{
    return pid;
}

bool CGIHandler::isStarted() const
{
    return started;
}

bool CGIHandler::isComplete() const
{
    return finished;
}

int CGIHandler::getStdinFd() const
{
    return in_fd[1];
}

int CGIHandler::getStdoutFd() const
{
    return out_fd[0];
}
