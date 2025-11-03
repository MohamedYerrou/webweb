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
     time_t _startTime;

public:
    CGIHandler(Client* c);
    ~CGIHandler();

    void startCGI(const std::string& scriptPath, const std::map<std::string,std::string>& env);
    void readOutput();
    bool isFinished() const;
    std::string getBuffer() const;
    int getOutFD() const;
    time_t getStartTime() const { return _startTime; }
    bool    isStarted() const { return started; }
};

#endif
