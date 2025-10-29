#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <vector>
#include <map>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <unistd.h>

class Client;

class CGIHandler
{
private:
    Client* client;
    int in_fd[2];
    int out_fd[2];
    pid_t pid;
    bool started;
    bool finished;
    std::string buffer;
    size_t bytes_written;
    bool _error;

public:
    CGIHandler(Client* c);
    ~CGIHandler();

    void startCGI(const std::string& scriptPath, const std::map<std::string,std::string>& env);
    void readOutput();
    bool isFinished() const;
    std::string getBuffer() const;
    int getOutFD() const;

    void setError(bool error);
    size_t getBytesWritten() const;
    void addBytesWritten(size_t bytes);
    void appendResponse(const char* buf, ssize_t length);
    void setComplete(bool complete);
    int getPid() const;
    bool isStarted() const;
    bool isComplete() const;
    int getStdinFd() const;
    int getStdoutFd() const;
    bool is_Error() const;


};

std::string getExtension(const std::string& path);

#endif
