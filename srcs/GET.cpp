#include "../includes/Client.hpp"

void    Client::handleRedirection()
{
    std::pair<int, std::string> redir = location->getRedirection();
    std::string TextStatus = getStatusText(redir.first);
    std::string fullUrl = redir.second;
    if (fullUrl.find("http://") != 0 && fullUrl.find("https://") != 0)
        fullUrl.insert(0, "http://");

    std::stringstream body;
    body << "<!DOCTYPE HTML>"
        << "<html><head><title>" << redir.first << " " << TextStatus << "</title></head>"
        << "<body><h1>" << TextStatus << "</h1>"
        << "<p>Redirecting to: <a target=\"_blank\" href=\"" << fullUrl << "\">" << fullUrl << "</a></p>"
        << "</body></html>";
    
    std::string bodyStr = body.str();
    currentResponse = Response();
    currentResponse.setProtocol(currentRequest->getProtocol());
    currentResponse.setStatus(redir.first, TextStatus);
    currentResponse.setHeaders("Location", fullUrl);
    currentResponse.setHeaders("Content-Type", "text/html");
    currentResponse.setHeaders("Content-Length", intTostring(bodyStr.length()));
    currentResponse.setHeaders("Date", currentDate());
    currentResponse.setHeaders("Connection", "close");
    currentResponse.setBody(bodyStr);
}

void    Client::listingDirectory(std::string path)
{
    DIR* dirStream = opendir(path.c_str());
    if (!dirStream)
    {
        errorResponse(500, "Could not open this directory");
    }
    std::string buffer;
    buffer +=  "<!DOCTYPE html><html><head><title>Listing directory"
        "</title></head><body><h1> Listing directory: " + path + "</h1><ul>";
    struct dirent* entry;
    while ((entry = readdir(dirStream)) != NULL)
    {
        std::string url = currentRequest->getPath();
        if (url[url.length() - 1] != '/')
            url += '/';
        std::string fullPath = url + entry->d_name;
        buffer += "<li><a href=\"" + fullPath + "\">";
        buffer += entry->d_name;
        buffer += "</a></li>";
    }
    buffer += "</ul></body></html>";
    closedir(dirStream);
    currentResponse = Response();
    currentResponse.setProtocol(currentRequest->getProtocol());
    currentResponse.setStatus(200, "ok");
    currentResponse.setBody(buffer);
    currentResponse.setHeaders("Content-Type", "text/html");
    currentResponse.setHeaders("Content-Length", intTostring(buffer.length()));
    currentResponse.setHeaders("Date", currentDate());
    currentResponse.setHeaders("Connection", "close");
}

void    Client::PrepareResponse(const std::string& path)
{
    if (!fileOpened)
    {
        fileStream.open(path.c_str(), std::ios::binary);
        if (!fileStream.is_open())
        {
            errorResponse(500, strerror(errno));
            return;
        }
        fileOpened = true;
    }
    currentResponse = Response();
    currentResponse.setProtocol(currentRequest->getProtocol());
    currentResponse.setStatus(200, "OK");
    currentResponse.setHeaders("Content-Type", getMimeType(path));
    currentResponse.setHeaders("Content-Length", intTostring(getFileSize(path)));
    currentResponse.setHeaders("Date", currentDate());
    currentResponse.setHeaders("Connection", "close");
}

void    Client::handleDirectory(const std::string& path)
{
    std::string indexPath = path;
    if (indexPath[indexPath.length() - 1] != '/')
        indexPath += '/';
    size_t length = indexPath.length();
    std::vector<std::string> index = location->getIndex();
    std::vector<std::string>::iterator it;
    bool    foundFile = false;
    for (it = index.begin(); it != index.end(); it++)
    {
        std::string index = *it;
        indexPath += index;
        if (isFile(indexPath))
        {
            PrepareResponse(indexPath);
            foundFile = true;
            break;
        }
        indexPath.erase(length, indexPath.length());
    }
    if (!foundFile && location->getAutoIndex())
        listingDirectory(path);
    else if (!foundFile)
        errorResponse(403, "Forbiden serving this directory");
}

void    Client::handleGET()
{
    if (location)
    {
        if (!allowedMethod("GET"))
        {
            errorResponse(405, "Method not allowed");
            return;
        }
        if (location->hasRedir())
        {
            handleRedirection();
            return;
        }
        if (location->getRoot().empty())
        {
            errorResponse(500, "Missing root directive");
            return;
        }
        std::string totalPath = joinPath();
        if (isDir(totalPath))
        {
            
            handleDirectory(totalPath);
        }
        else if (isFile(totalPath))
        {
            
            std::cout <<  "============ reached here ==========="  << std::endl;
            PrepareResponse(totalPath);
        }
        else
            errorResponse(404, "NOT FOUND");
    }
}