#include "../includes/CGIHandler.hpp"
#include "../includes/Client.hpp"
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

    if (pid > 0)
    {
        int status;
        if (waitpid(pid, &status, WNOHANG) <= 0)
        { 
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
    std::map<std::string, std::string> queries = currentRequest->getQueries();
    if (!queries.empty())
    {
        std::string str;
        std::map<std::string, std::string>::const_iterator it;
        for (it = queries.begin(); it != queries.end(); ++it)
        {
            str += it->first;
            str += '=';
            str += it->second;
            str += '&';
        }
        env.push_back("QUERY_STRING=" + str);
    }
    
    if (headers.find("host") != headers.end())
    {
        std::string host = headers["host"];
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
    }
    else
    {
        env.push_back("SERVER_NAME=localhost");
        env.push_back("SERVER_PORT=80");
    }
    
    if (headers.find("content-type") != headers.end())
        env.push_back("CONTENT_TYPE=" + headers["content-type"]);
    else
        env.push_back("CONTENT_TYPE=");
    
    if (headers.find("content-length") != headers.end())
        env.push_back("CONTENT_LENGTH=" + headers["content-length"]);
    else
        env.push_back("CONTENT_LENGTH=0");
    
    env.push_back("REMOTE_ADDR=127.0.0.1");
    env.push_back("REMOTE_HOST=127.0.0.1");
    
    env.push_back("REDIRECT_STATUS=200");
    if (currentRequest->getMethod() == "POST")
    {
        std::string fileName = currentRequest->getFileName();
        env.push_back("POST_DATA_FILE=" + fileName);
    }

    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
    {
        std::string headerName = it->first;
        std::string headerValue = it->second;
        
        if (headerName == "content-type" || headerName == "content-length" || 
            headerName == "authorization" || headerName == "connection")
            continue;
        
        std::string metaVarName = "HTTP_";
        for (size_t i = 0; i < headerName.length(); ++i)
        {
            if (headerName[i] == '-')
                metaVarName += '_';
            else
                metaVarName += std::toupper(headerName[i]);
        }
        
        env.push_back(metaVarName + "=" + headerValue);
    }

    return env;
}

void Client::handleCGI()
{
    std::vector<std::string> vecEnv = buildCGIEnv(newPath);
    std::map<std::string,std::string> envMap;
    for (std::size_t i = 0; i < vecEnv.size(); ++i)
    {
        std::size_t pos = vecEnv[i].find('=');
        if (pos != std::string::npos)
            envMap[vecEnv[i].substr(0, pos)] = vecEnv[i].substr(pos + 1);
    }

    if (!cgiHandler)
    {
        cgiHandler = new CGIHandler(this);
        try
        {
            cgiHandler->startCGI(newPath, envMap);
        }
        catch (const std::exception& e)
        {
            errorResponse(500, e.what());
            delete cgiHandler;
            cgiHandler = NULL;
            return;
        }
    }

    if (cgiHandler)
        cgiHandler->readOutput();
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
        if (in_fd[0] != -1) { close(in_fd[0]); in_fd[0] = -1; }
        if (in_fd[1] != -1) { close(in_fd[1]); in_fd[1] = -1; }
        if (out_fd[0] != -1) { close(out_fd[0]); out_fd[0] = -1; }
        if (out_fd[1] != -1) { close(out_fd[1]); out_fd[1] = -1; }
        if (err_pipe[0] != -1) { close(err_pipe[0]); err_pipe[0] = -1; }
        if (err_pipe[1] != -1) { close(err_pipe[1]); err_pipe[1] = -1; }
        throw std::runtime_error("pipe failed");
    }

    if (fcntl(err_pipe[1], F_SETFD, FD_CLOEXEC) == -1)
    {
        close(in_fd[0]); close(in_fd[1]);
        close(out_fd[0]); close(out_fd[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        throw std::runtime_error("fcntl failed");
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
        dup2(out_fd[1], STDERR_FILENO);
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
        
        if (access(scriptPath.c_str(), F_OK) != 0) {
            write(out_fd[1], "E", 1);
            for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
            exit(1);
        }
        std::map<std::string, std::string> cgi = (client->findMathLocation(client->getCurrentRequest()->getPath())->getCgi());

        std::string interpreter;
        std::size_t dotPos = scriptPath.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            std::string ext = scriptPath.substr(dotPos);
            std::map<std::string, std::string>::const_iterator it = cgi.find(ext);
            if (it != cgi.end())
            interpreter = it->second;
        }
        
        if (!interpreter.empty())
        {
            char* argv[] = { const_cast<char*>(interpreter.c_str()), const_cast<char*>(scriptPath.c_str()), NULL };
            execve(interpreter.c_str(), argv, envp.empty() ? NULL : &envp[0]);
        }
        else
        {
            char* argv[] = { const_cast<char*>(scriptPath.c_str()), NULL };
            execve(scriptPath.c_str(), argv, envp.empty() ? NULL : &envp[0]);
        }
        write(out_fd[1], "E", 1);
        for (size_t i = 0; i < envp.size(); ++i) delete[] envp[i];
        exit(1);
    }
    else
    {
        close(in_fd[0]);
        close(out_fd[1]);
        close(err_pipe[1]);
        char err = 0;
        ssize_t n = read(err_pipe[0], &err, 1);
        close(err_pipe[0]);
        if (n == 1)
        {
            int status;
            waitpid(pid, &status, WNOHANG);
            pid = -1;
            if (in_fd[1] != -1)
            {
                close(in_fd[1]);
                in_fd[1] = -1;
            }
            if (out_fd[0] != -1)
            {
                close(out_fd[0]);
                out_fd[0] = -1;
            }
            throw std::runtime_error(std::string("execve failed for ") + scriptPath);
        }

        fcntl(out_fd[0], F_SETFL, O_NONBLOCK);
        started = true;
        _startTime = time(NULL);
    }
}


void CGIHandler::readOutput()
{
    if (!started || finished || out_fd[0] == -1)
        return;
    char buf[4096];

    while (true)
    {
        ssize_t n = read(out_fd[0], buf, sizeof(buf));
        if (n > 0)
        {
            buffer.append(buf, n);
            continue;
        }
        else if (n == 0)
        {
            finished = true;
            if (pid > 0)
            {
                waitpid(pid, NULL, WNOHANG);
                pid = -1;
            }
            close(out_fd[0]);
            out_fd[0] = -1;
            break;
        }
        else
            break;
    }
}

void Client::checkCGIValid()
{
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
        return errorResponse(405, "METHOD NOT ALLOWED");

    const std::map<std::string, std::string>& cgiMap = location->getCgi();
    bool cgiConfigured = !cgiMap.empty();

    if (isDir(newPath))
    {
        const std::vector<std::string>& indexFiles = location->getIndex();
        bool indexFound = false;
        std::string originalPath = newPath;

        if (!indexFiles.empty())
        {
            if (originalPath.length() > 0 && originalPath[originalPath.length() - 1] != '/')
                originalPath += "/";

            for (size_t i = 0; i < indexFiles.size(); ++i)
            {
                std::string indexPath = originalPath + indexFiles[i];

                if (isFile(indexPath))
                {
                    newPath = indexPath;
                    indexFound = true;
                    break;
                }
            }
        }

        if (!indexFound)
        {
            if (location->getAutoIndex())
                return listingDirectory(newPath);
            else
                return errorResponse(403, "FORBIDDEN");
        }
    }
    else if (!isFile(newPath))
        return errorResponse(404, "NOT FOUND");

    if (cgiConfigured)
    {
        std::size_t dotPos = newPath.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            std::string ext = newPath.substr(dotPos);
            std::map<std::string, std::string>::const_iterator it = cgiMap.find(ext);
            if (it != cgiMap.end())
            {
                setIsCGI(true);
                return;
            }
        }
    }
    setIsCGI(false);
    if (currentRequest->getMethod() == "GET")
        handleGET();
    // else if (currentRequest->getMethod() == "POST")
    //     handlePOST();
}