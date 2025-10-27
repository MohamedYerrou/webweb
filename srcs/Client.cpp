#include "../includes/Client.hpp"
#include <errno.h> // For errno

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
      isCGI(false),
      // --- Initialize new CGI members ---
      cgi_out_fd(-1),
      cgi_in_fd(-1),
      cgi_body_bytes_written(0),
      cgi_body_sent(false),
      cgi_headers_parsed(false)
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
    // This is for non-multipart POST
    oneBody = true;
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
        // This is for static files.
        // We must prepare the response headers here.
        PrepareResponse(newPath); // Assuming newPath is set
        return;
    }

    char    buffer[1024];
    fileStream.read(buffer, sizeof(buffer));
    size_t bytesRead = fileStream.gcount();
    
    if (bytesRead > 0)
    {
        currentResponse.setBody(std::string(buffer, bytesRead));
    }
    
    if (bytesRead < sizeof(buffer) || fileStream.eof() || fileStream.fail())
    {
        sentAll = true;
        fileStream.close();
        fileOpened = false;
    }
}


const Location* Client::findMathLocation(std::string url)
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

// Modified to call setupCGI
void Client::handleCompleteRequest()
{
    location = findMathLocation(currentRequest->getPath());
    if (!location) {
        errorResponse(404, "Not Found");
        return;
    }
    newPath = joinPath();
    
    checkCGIValid(); // This sets isCGI
    if (getIsCGI())
    {
        setupCGI(); // Create CGI process and get FDs
        // If setupCGI failed, it will set an errorResponse and set isCGI=false
    }
    
    else if (!getIsCGI()) // Fallback for static or if CGI setup failed
    {
        if (currentRequest->getMethod() == "GET")
            handleGET();
        else if (currentRequest->getMethod() == "DELETE")
            handleDELETE();
        // POST for file upload is handled in appendData/handleBody
        // If it was a failed CGI, an error response is already set.
    }
}


void    Client::errorResponse(int code, const std::string& error)
{
    if (!location)
        location = findMathLocation(currentRequest->getPath());
    
    // Check for custom error page
    if (location)
    {
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
                    currentResponse.setProtocol("HTTP/1.1");
                    currentResponse.setStatus(code, getStatusText(code));
                    currentResponse.setHeaders("Content-Type", getMimeType(path));
                    currentResponse.setHeaders("Content-Length", intTostring(body.length()));
                    currentResponse.setHeaders("Date", currentDate());
                    currentResponse.setHeaders("Connection", "close");
                    currentResponse.setBody(body);
                    file.close();
                    sentAll = true; // Mark as ready to send in one go
                    return;
                }
            }
        }
    }

    // Default error page
    std::stringstream body;
    body << "<!DOCTYPE HTML>"
        << "<html><head><title>"<< code << " - " << getStatusText(code) << "</title></head>"
        << "<body><h1>"<< code << " - " << getStatusText(code) << "</h1>"
        << "<p>" << error << "</p>"
        << "</body></html>";

    std::string bodyStr = body.str();
    currentResponse = Response();
    currentResponse.setProtocol("HTTP/1.1");
    currentResponse.setStatus(code, getStatusText(code));
    currentResponse.setHeaders("Content-Type", "text/html");
    currentResponse.setHeaders("Content-Length", intTostring(bodyStr.length()));
    currentResponse.setHeaders("Date", currentDate());
    currentResponse.setHeaders("Connection", "close");
    currentResponse.setBody(bodyStr);
    sentAll = true; // Mark as ready to send in one go
}

void    Client::handleHeaders(const std::string& raw)
{
    std::cout << "Header request" << std::endl;
    // std::cout << raw << std::endl; // Debug
    try
    {
        currentRequest = new Request();
        currentRequest->parseRequest(raw);
        // parsedRequest(*currentRequest);
        bodySize = currentRequest->getContentLength();
        if (bodySize > currentServer->getMaxSize())
        {
            errorResponse(413, "Payload Too Large");
            reqComplete = true;
            requestError = true;
        }
        else if (bodySize == 0 && currentRequest->getMethod() == "POST")
        {
            // Allow POST with 0 body for some CGI
            // errorResponse(411, "Length Required");
            // reqComplete = true;
            hasBody = false;
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
        errorResponse(currentRequest ? currentRequest->getStatusCode() : 400, e.what());
    }
}

void    Client::handleBody(const char* buf, ssize_t length)
{
    // std::cout << "BUFFER: " << buf << std::endl;
    size_t toAppend = std::min((size_t)length, bodySize);
    
    if (oneBody)
    {
        // This is a simple POST, save body to request string
        currentRequest->appendBody(buf, toAppend);
    }
    else
    {
        // This is multipart, handle file upload
        if (inBody && !finishBody)
        {
            body.append(buf, toAppend);
            if (body.find(endBoundry) != std::string::npos)
            {
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
        sentAll = true; // Mark as ready
        if (!oneBody)
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
    const Location* loc = findBestMatch(uri);
    if (!loc)
    {
        errorResponse(500, "Internal Server Error: No location match");
        reqComplete = true;
        return "";
    }
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
                if (reqComplete) // Error happened in handleHeaders
                    return;
                
                getContentType(); // This sets oneBody flag
                
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


// ====================================================================
// This function was in CGIHandler.cpp, it belongs here
// ====================================================================
std::vector<std::string> Client::buildCGIEnv(const std::string& scriptPath)
{
    std::vector<std::string> env;
    
    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=" + currentRequest->getProtocol());
    env.push_back("REQUEST_METHOD=" + currentRequest->getMethod());
    env.push_back("SCRIPT_FILENAME=" + scriptPath);
    
    // Nginx-style PATH_INFO and SCRIPT_NAME
    env.push_back("SCRIPT_NAME=" + currentRequest->getPath()); // Path from URL
    env.push_back("PATH_INFO=" + currentRequest->getPath()); // Simple version
    env.push_back("QUERY_STRING=" + currentRequest->getQueryString());
    
    std::map<std::string, std::string> headers = currentRequest->getHeaders();
    
    // Add all HTTP headers as HTTP_...
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
    {
        std::string key = it->first;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        std::replace(key.begin(), key.end(), '-', '_');
        env.push_back("HTTP_" + key + "=" + it->second);
    }

    // Body-related vars
    if (headers.find("content-type") != headers.end())
        env.push_back("CONTENT_TYPE=" + headers["content-type"]);

    if (headers.find("content-length") != headers.end())
        env.push_back("CONTENT_LENGTH=" + headers["content-length"]);

    env.push_back("SERVER_SOFTWARE=webserv/1.0");
    env.push_back("REDIRECT_STATUS=200"); // For PHP-CGI

    return env;
}

// ====================================================================
// NEW/MODIFIED CGI METHODS
// ====================================================================

CGIHandler* Client::getCGIHandler() { return cgiHandler; }
int     Client::getCGIOutFD() const { return cgi_out_fd; }
int     Client::getCGIInFD() const { return cgi_in_fd; }
void    Client::setCGIOutFD(int fd) { cgi_out_fd = fd; }
void    Client::setCGIInFD(int fd) { cgi_in_fd = fd; }
bool    Client::isCGIBodySent() const { return cgi_body_sent; }
void    Client::setCGIBodySent(bool val) { cgi_body_sent = val; }
size_t* Client::getCGIBytesWrittenPtr() { return &cgi_body_bytes_written; }
bool    Client::areCGIHeadersParsed() const { return cgi_headers_parsed; }

std::string& Client::getCGIResponseBuffer()
{
    return cgi_response_buffer;
}

void Client::cleanupCGI()
{
	if (cgiHandler)
	{
		delete cgiHandler;
		cgiHandler = NULL;
	}
	cgi_in_fd = -1;
	cgi_out_fd = -1;
}

// New function to parse CGI headers from the cgi_response_buffer
void Client::parseCGIHeaders()
{
    std::size_t pos = cgi_response_buffer.find("\r\n\r\n");
    if (pos == std::string::npos)
    {
        // Headers not fully received yet
        return;
    }

    std::string headers_part = cgi_response_buffer.substr(0, pos);
    std::string body_part = cgi_response_buffer.substr(pos + 4);

    cgi_headers_parsed = true;

    // Build the HTTP response
    currentResponse = Response();
    currentResponse.setProtocol(currentRequest->getProtocol());
    currentResponse.setStatus(200, "OK"); // Default

    std::stringstream ss(headers_part);
    std::string line;
    while (std::getline(ss, line))
    {
        if (line.empty() || line[0] == '\r')
            continue;
        if (line.size() > 0 && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        std::size_t colon_pos = line.find(":");
        if (colon_pos != std::string::npos)
        {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Simple trim for leading space
            if (!value.empty() && value[0] == ' ')
                value.erase(0, 1);
            
            if (key == "Status")
            {
                std::size_t code_pos = value.find(' ');
                if (code_pos != std::string::npos)
                {
                    int code = std::atoi(value.substr(0, code_pos).c_str());
                    std::string text = value.substr(code_pos + 1);
                    currentResponse.setStatus(code, text);
                }
            }
            else
            {
                currentResponse.setHeaders(key, value);
            }
        }
    }
    
    currentResponse.setHeaders("Connection", "close");
    
    // Put the first part of the body back into the buffer
    cgi_response_buffer = body_part;
}


// This replaces your old handleCGI()
void Client::setupCGI()
{
    // 1. Build environment
    std::vector<std::string> vecEnv = buildCGIEnv(newPath);
    std::map<std::string,std::string> envMap;
    for (std::size_t i = 0; i < vecEnv.size(); ++i)
    {
        std::size_t pos = vecEnv[i].find('=');
        if (pos != std::string::npos)
            envMap[vecEnv[i].substr(0, pos)] = vecEnv[i].substr(pos + 1);
    }
    
    // Add the path to the interpreter
    const std::map<std::string, std::string>& cgiMap = location->getCgi();
    std::size_t dotPos = newPath.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        std::string ext = newPath.substr(dotPos);
        std::map<std::string, std::string>::const_iterator it = cgiMap.find(ext);
        if (it != cgiMap.end())
        {
            envMap["CGI_INTERPRETER"] = it->second;
        }
    }


    // 2. Start CGI process
    if (cgiHandler) // Should not happen
        delete cgiHandler;
        
    cgiHandler = new CGIHandler(this);
    try
    {
        cgiHandler->startCGI(newPath, envMap);
        cgi_out_fd = cgiHandler->getOutFD();
        cgi_in_fd = cgiHandler->getInFD();

        // If request has no body, close CGI's stdin immediately
        if (currentRequest->getMethod() != "POST" || currentRequest->getContentLength() == 0)
        {
            cgiHandler->closeInFD();
            cgi_in_fd = -1; // Mark as closed
            cgi_body_sent = true;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[CGI] Exception while starting CGI: " << e.what() << std::endl;
        errorResponse(500, e.what()); // This sets up an error response
        delete cgiHandler;
        cgiHandler = NULL;
        cgi_out_fd = -1;
        cgi_in_fd = -1;
        isCGI = false; // Fallback to sending the error response statically
    }
}

// This function was in CGIHandler.cpp
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