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

public:
    CGIHandler(Client* c);
    ~CGIHandler();

    void startCGI(const std::string& scriptPath, const std::map<std::string,std::string>& env);
    void readOutput(); // Reads from out_fd[0] into buffer
    void closeInFD();  // Added: Closes in_fd[1]

    bool isFinished() const;
    void setFinished(bool val); // Added
    std::string getBuffer() const;
    void clearBuffer(); // Added
    int getOutFD() const;
    int getInFD() const; // Added
};

std::string getExtension(const std::string& path);

#endif