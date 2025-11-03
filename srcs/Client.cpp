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
    lastActivityTime = time(NULL);
	isRequesting = true;
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

void			Client::setLastActivityTime(time_t time)
{
    lastActivityTime = time;
}

void			Client::setIsRequesting(bool flag)
{
    isRequesting = flag;
}

bool			Client::isTimedOut()
{
    time_t currentTime = time(NULL);
    time_t elapsed = currentTime - lastActivityTime;
    if (isRequesting)
        return elapsed > REQUEST_TIMEOUT;
    else
        return elapsed > RESPONSE_TIMEOUT;
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

void Client::handleCompleteRequest()
{
    location = findMathLocation(currentRequest->getPath());
    newPath = joinPath();
    if (location->getPATH() == "/cgi")
        checkCGIValid();
    if (currentRequest->getMethod() == "GET")
        handleGET();
    else if (currentRequest->getMethod() == "DELETE")
        handleDELETE();
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
                    std::cout << "TARGETPATH: " << target_path << std::endl;
                    if (target_path.empty() && reqComplete)
                        return;
                    currentRequest->generateTmpFile(target_path, "");
                }
                std::string bodyStart = headers.substr(headerPos);
                handlePost(bodyStart.c_str(), bodyStart.length());
            }
        }
    }
    else
    {
        if (hasBody && !reqComplete)
            handlePost(buf, length);
    }
}