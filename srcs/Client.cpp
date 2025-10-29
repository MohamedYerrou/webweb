#include "../includes/Client.hpp"

Client::Client(int fd, Server* srv)
    : fd(fd),
      bodySize(0),
      endHeaders(false),
      reqComplete(false),
      hasBody(false),
      requestError(false),
      currentRequest(NULL),
      currentServer(srv),
      location(NULL),
      sentAll(false),
      fileOpened(false),
      cgiHandler(NULL),
      isCGI(false)
{
    bodySize = 0;
    sentAll = false;
	fileOpened = false;
    oneBody = false;
    inBody = false;
    finishBody = false;
}

Client::~Client()
{
    if (fileOpened)
        fileStream.close();
    if (currentRequest != NULL)
        delete currentRequest;
    if (cgiHandler != NULL)
    {
        delete cgiHandler;
        cgiHandler = NULL;
    }
}

int Client::getFD() const
{
    return fd;
}

const std::string& Client::getHeaders() const
{
    return headers;
}

bool    Client::getEndHeaders() const
{
    return endHeaders;
}

bool    Client::getReqComplete() const
{
    return reqComplete;
}

bool    Client::getRequestError() const
{
    return requestError;
}

void    Client::setBodySize(size_t size)
{
    bodySize = size;
}

Request* Client::getRequest() const
{
    return currentRequest;
}

Response& Client::getResponse()
{
    return currentResponse;
}

bool				Client::getSentAll() const
{
    return sentAll;
}

void				Client::setSentAll(bool flag)
{
    sentAll = flag;
}

bool    			Client::getContentType()
{
    std::map<std::string, std::string>::const_iterator it = currentRequest->getHeaders().find("content-type");
    if (it !=  currentRequest->getHeaders().end())
    {
        std::string type = it->second;
        if (type.find("multipart/form-data") == 0)
        {
            std::size_t pos = type.find("boundary=");
            if (pos != std::string::npos)
            {
                boundary = type.substr(pos + 9);
                std::size_t semicolon = boundary.find(';');
                if (semicolon != std::string::npos)
                    boundary = boundary.substr(0, semicolon);
                if (boundary.length() >= 2 && boundary[0] == '"' && boundary[boundary.length() - 1] == '"')
                    boundary = boundary.substr(1, boundary.length() - 2);
                boundary = "--" + boundary;
                endBoundry = boundary + "--";
                return true;
            }
            else
            {
                errorResponse(400, "Invalid multipart body.");
                reqComplete = true;
                return false;
            }
        }
    }
    return false;
}

std::string    Client::joinPath()
{
    std::string updatePath = currentRequest->getPath();
    std::string root = location->getRoot();
    if (root[root.length() - 1] == '/')
        root.erase(root.length() - 1);
    if (location->getPATH() != "/")
    {
        if (updatePath.find(location->getPATH()) == 0)
        {
            updatePath = updatePath.substr(location->getPATH().length());
        }
    }
    std::string Total = root + updatePath;
    return Total;
}

bool    Client::allowedMethod(const std::string& method)
{
    std::vector<std::string> methods = location->getMethod();
    if (methods.empty())
        return false;
    std::vector<std::string>::iterator it;
    it = std::find(methods.begin(), methods.end(), method);
    if (it == methods.end())
        return false;
    return true;
}

void    Client::handleFile()
{
    if (!fileOpened)
    {
        sentAll = true;
        return;
    }
    char    buffer[1024];
    fileStream.read(buffer, sizeof(buffer));
    size_t bytesRead = fileStream.gcount();
    if (bytesRead == 0)
        sentAll = true;
    else
    {
        currentResponse.setBody(std::string(buffer, bytesRead));
        if (bytesRead < sizeof(buffer))
            sentAll = true;
    }
}

const Location*   Client::findMathLocation(std::string url)
{
    if (url[0] != '/') //this if got an error in parsing
        url.insert(0, "/");
    const std::vector<Location>& locations = currentServer->getLocations();
    if (locations.empty())
        return NULL;

    const Location* currentLocation = NULL;
    std::size_t prefix = 0;
	for (size_t i = 0; i < locations.size(); i++)
	{
        std::string locationPath = locations[i].getPATH();
        if (url == locationPath)
            return &locations[i];
        else
        {
            if (url.substr(0, locationPath.length()) == locationPath)
            {
                if (prefix < locationPath.length())
                {
                    prefix = locationPath.length();
                    currentLocation = &locations[i];
                }
            }
        }
	}
    return currentLocation;
}

std::string getExtension(const std::string& path)
{
    size_t dot_pos = path.rfind('.');
    if (dot_pos == std::string::npos)
        return "";
    size_t slash_pos = path.rfind('/');
    if (slash_pos != std::string::npos && slash_pos > dot_pos)
        return "";
    return path.substr(dot_pos);
}

void Client::handleCompleteRequest()
{
    location = findMathLocation(currentRequest->getPath());
    if (!location)
    {
        errorResponse(404, "Location not found");
        return;
    }
    // Check if this is a CGI request
    if (location->getPATH() == "/cgi") // adjust condition as needed
    {
        newPath = joinPath();
        checkCGIValid();
        if (getIsCGI())
            handleCGI();
    }
    // Handle regular HTTP methods
    if (currentRequest->getMethod() == "GET")
        handleGET();
    else if (currentRequest->getMethod() == "DELETE")
        handleDELETE();
    else if (currentRequest->getMethod() == "POST")
    {
        // POST without CGI - already handled in handleBody
        // or handle non-CGI POST here if needed
    }
}


void    Client::errorResponse(int code, const std::string& error)
{
    if (!location)
        location = findMathLocation(currentRequest->getPath());
    std::map<int, std::string> errors = location->getErrors();
    std::map<int, std::string>::iterator it = errors.find(code);
    if (it != errors.end() && !it->second.empty())
    {
        std::string errorPage = it->second;
        std::string path = location->getRoot();
        if (!path.empty() && path[path.length() - 1] == '/')
            path.erase(path.length() - 1);
        if (errorPage[0] != '/')
            errorPage.insert(0, "/");
        path += errorPage;
        if (isFile(path))
        {
            std::ifstream file((path).c_str(), std::ios::binary);
            if (file.is_open())
            {
                std::stringstream buffer;
                buffer << file.rdbuf();
                std::string body = buffer.str();
                currentResponse = Response();
                currentResponse.setProtocol("HTTP/1.0");
                currentResponse.setStatus(code, getStatusText(code));
                currentResponse.setHeaders("Content-Type", getMimeType(path));
                currentResponse.setHeaders("Content-Length", intTostring(body.length()));
                currentResponse.setHeaders("Date", currentDate());
                currentResponse.setHeaders("Connection", "close");
                currentResponse.setBody(body);
                file.close();
                return;
            }
        }
    }
    std::stringstream body;
    body << "<!DOCTYPE HTML>"
        << "<html><head><title>"<< code << " - " << getStatusText(code) << "</title></head>"
        << "<body><h1>"<< code << " - " << getStatusText(code) << "</h1>"
        << "<p>" << error << "</p>"
        << "</body></html>";

    std::string bodyStr = body.str();
    currentResponse = Response();
    currentResponse.setProtocol("HTTP/1.0");
    currentResponse.setStatus(code, getStatusText(code));
    currentResponse.setHeaders("Content-Type", "text/html");
    currentResponse.setHeaders("Content-Length", intTostring(bodyStr.length()));
    currentResponse.setHeaders("Date", currentDate());
    currentResponse.setHeaders("Connection", "close");
    currentResponse.setBody(bodyStr);
}

void    Client::handleHeaders(const std::string& raw)
{
    std::cout << "Header request" << std::endl;
    std::cout << raw << std::endl;
    try
    {
        currentRequest = new Request();
        currentRequest->parseRequest(raw);
        // parsedRequest(*currentRequest);
        bodySize = currentRequest->getContentLength();
        // if (bodySize > currentServer->getMaxSize())
        // {
        //     errorResponse(413, "Payload Too Large");
        //     reqComplete = true;
        // }
        if (bodySize == 0 && currentRequest->getMethod() == "POST")
        {
            errorResponse(411, "Length Required");
            reqComplete = true;
        }
        else if (bodySize > 0 && currentRequest->getMethod() == "POST")
            hasBody = true;
        else
            reqComplete = true;
    } catch (const std::exception& e)
    {
        reqComplete = true;
        requestError = true;
        errorResponse(currentRequest->getStatusCode(), e.what());
        // std::cout << "Request error: " << e.what() << std::endl;
    }
}

void    Client::handleBody(const char* buf, ssize_t length)
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
        else
        {
            body.append(buf, toAppend);
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

const Location* Client::findBestMatch(const std::string uri)
{
    int index = -1;
    const std::vector<Location>& locations = currentServer->getLocations();

    for (size_t i = 0; i < locations.size(); i++)
    {
        std::string loc = locations[i].getPATH();

        if (uri.compare(0, loc.length(), loc) == 0
            && (uri.length() == loc.length() || uri[loc.length()] == '/' || loc == "/"))
        {
            if (index != -1 && locations[index].getPATH().length() <  locations[i].getPATH().length())
                index = i;
            else if (index == -1)
                index = i;
        }
    }
    if (index == -1)
        return NULL;
    return &locations[index];
}

std::string Client::constructFilePath(std::string uri)
{
    std::string path;
    const Location*   loc = findBestMatch(uri);
    std::string uploadFile = loc->getUploadStore();
    if (uploadFile.empty())
    {
        errorResponse(500, "MISSING UPLOAD STORE");
        reqComplete = true;
        return "";
    }
    path = uploadFile + uri.erase(0, loc->getPATH().length());
    return path;
}

void    Client::appendData(const char* buf, ssize_t length)
{
    if (!endHeaders)
    {
        headers.append(buf, length);
        std::size_t headerPos = headers.find("\r\n\r\n");
        if (headerPos != std::string::npos)
        {
            endHeaders = true;
            headerPos += 4;
            handleHeaders(headers.substr(0, headerPos));
            size_t bodyInHeader = headers.length() - headerPos;
            if (hasBody && bodyInHeader > 0)
            {
                if (!getContentType())
                {
                    if (reqComplete)
                        return;
                    oneBody = true;
                    std::string target_path = constructFilePath(currentRequest->getPath());
                    if (target_path.empty() && reqComplete)
                        return;
                    currentRequest->generateTmpFile(target_path, "");
                }
                std::string bodyStart = headers.substr(headerPos);
                handleBody(bodyStart.c_str(), bodyStart.length());
            }
        }
    }
    else
    {
        if (hasBody && !reqComplete)
            handleBody(buf, length);
    }
}







void Client::setCGIError(bool error)
{
    if (cgiHandler)
        cgiHandler->setError(error);
}

size_t Client::getCGIBytesWritten() const
{
    if (cgiHandler)
        return cgiHandler->getBytesWritten();
    return 0;
}

void Client::addCGIBytesWritten(size_t bytes)
{
    if (cgiHandler)
        cgiHandler->addBytesWritten(bytes);
}

void Client::appendCGIResponse(const char* buf, ssize_t length)
{
    if (cgiHandler)
        cgiHandler->appendResponse(buf, length);
}

void Client::setCGIComplete(bool complete)
{
    if (cgiHandler)
        cgiHandler->setComplete(complete);
}




Server* Client::getServer() const
{
    return currentServer;
}


CGIHandler* Client::getCGIHandler()
{
    return cgiHandler;
}