#include "../includes/Client.hpp"

void    Client::handleInBody()
{
    if (body.find(endBoundry) != std::string::npos)
    {
        // std::cout << "BODY FINISHED" << std::endl;
        std::string leftPart = body.substr(0, body.find(endBoundry));
        currentRequest->appendBody(leftPart.c_str(), leftPart.length());
        body.erase(0, body.find(endBoundry));
        finishBody = true;
    }
    std::size_t nextBoundary = body.find(boundary);
    if (nextBoundary != std::string::npos)
    {
        std::string firstPart = body.substr(0, nextBoundary);
        currentRequest->appendBody(firstPart.c_str(), firstPart.length());
        // body.erase(0, nextBoundary);
        inBody = false;
        currentRequest->closeFileUpload();
    }
    else
    {
        size_t safeZone = body.length() - boundary.length();
        if (safeZone > 0)
        {
            std::string safePart = body.substr(0, safeZone);
            currentRequest->appendBody(safePart.c_str(), safePart.length());
            body.erase(0, safeZone);
        }
    }
}

void    Client::handleBodyHeaders()
{
    std::size_t endBodyHeaders = body.find("\r\n\r\n");
    if (endBodyHeaders != std::string::npos)
    {
        inBody = true;
        endBodyHeaders += 4;
        std::string bodyStart = body.substr(endBodyHeaders);
        // body.erase(0, boundary.length());
        std::size_t pos = body.find("filename=");
        if (pos != std::string::npos)
        {
            pos += 10;
            std::size_t nextPos = body.find("\r\n", pos);
            if (nextPos == std::string::npos)
                return;
            std::string filename = body.substr(pos, nextPos - pos - 1);
            std::cout << "FILENAME: " << filename << std::endl;
            std::string target_path = constructFilePath(currentRequest->getPath());
            if (target_path.empty() && reqComplete)
                return;
            currentRequest->generateTmpFile(target_path, filename);
            currentRequest->appendBody(bodyStart.c_str(), bodyStart.length());
        }
        else
        {
            //TODO: handle with default file name;
            std::string target_path = constructFilePath(currentRequest->getPath());
            if (target_path.empty() && reqComplete)
                return;
            currentRequest->generateTmpFile(target_path, "");
            currentRequest->appendBody(bodyStart.c_str(), bodyStart.length());
        }
        body.clear();
    }
}

void    Client::handlePost(const char* buf, ssize_t length)
{
    // std::cout << "BUFFER: " << buf << std::endl;
    size_t toAppend = std::min((size_t)length, bodySize);
    if (oneBody)
        currentRequest->appendBody(buf, toAppend);
    else
    {
        if (inBody && !finishBody)
        {
            body.append(buf, toAppend);
            handleInBody();
        }
        else
        {
            body.append(buf, toAppend);
            handleBodyHeaders();
        }
    }
    bodySize -= toAppend;
    if (bodySize <= 0)
    {
        std::cout << "BODY SIZE IS: " << bodySize << std::endl;
        currentResponse = Response();
        std::string bodyStr = "Upload done.";
        currentResponse.setProtocol(currentRequest->getProtocol());
        currentResponse.setStatus(200, "OK");
        currentResponse.setHeaders("Content-Type", "text/plain");
        currentResponse.setHeaders("Content-Length", intTostring(bodyStr.length()));
        currentResponse.setHeaders("Date", currentDate());
        currentResponse.setHeaders("Connection", "close");
        currentResponse.setBody(bodyStr);
        currentRequest->closeFileUpload();
        reqComplete = true;
    }
}