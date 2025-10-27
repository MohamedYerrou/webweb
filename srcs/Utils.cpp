#include "../includes/Utils.hpp"
#include "../includes/Request.hpp"

std::string currentDate()
{
    time_t now = time(NULL);
    struct tm* timeInfo = gmtime(&now);
    char buffer[100];
    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", timeInfo);
    return std::string(buffer);
}

std::string intTostring(size_t value)
{
    std::string result;
    std::stringstream ss;
    ss << value;
    ss >> result;
    return result;
}

int stringToInt(const std::string& str, int base)
{
    std::stringstream ss(str);
    int result;
    if (base == 16)
        ss >> std::hex >> result;
    else
        ss >> result;
    return result;
}

std::string trim(std::string str)
{
    std::size_t start = str.find_first_not_of(" \t");
    if (start == std::string::npos)
        return "";
    std::size_t end = str.find_last_not_of(" \t");
    return (str.substr(start, end - start + 1));
}

void    toLowerCase(std::string& str)
{
    for (size_t i = 0; i < str.length(); i++)
        str[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(str[i])));
}

bool    isFile(const std::string& path)
{
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) == -1)
        return false;
    return S_ISREG(fileStat.st_mode);
}

size_t  getFileSize(const std::string& path)
{
    struct stat sb;
    stat(path.c_str(), &sb);
    return (sb.st_size);
}

bool    isDir(const std::string& path)
{
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) == -1)
        return false;
    return S_ISDIR(fileStat.st_mode);
}

bool    allowedDelete(std::string path)
{
    std::size_t pos = path.find_last_of('/');
    std::string parentDir = path.substr(0, pos);
    if (access(parentDir.c_str(), W_OK | X_OK) != 0)
        return false;
    return true;
}

bool    isEmpty(const std::string& path)
{
    DIR* dirStram = opendir(path.c_str());
    if (!dirStram)
        return false;
    struct dirent* entry;
    bool empty = true;
    while ((entry = readdir(dirStram)) != NULL)
    {
        if (std::string(entry->d_name) != "." && std::string(entry->d_name) != "..")
        {
            empty = false;
            break;
        }
    }
    closedir(dirStram);
    return empty;
}

std::string getStatusText(int code)
{
    std::string text;
    switch (code)
    {
        case 200:
            text = "OK";
            break;
        case 201:
            text = "Created";
            break;
        case 204:
            text = "No Content";
            break;
        case 301:
            text = "Moved Permanently";
            break;
        case 400:
            text = "Bad Request";
            break;
        case 403:
            text = "Forbidden";
            break;
        case 404:
            text = "Not Found";
            break;
        case 405:
            text = "Method Not Allowed";
            break;
        case 408:
            text = "Request Timeout";
            break;
        case 409:
            text = "Conflict";
            break;
        case 411:
            text = "Length Required";
            break;
        case 413:
            text = "Payload Too Large";
            break;
        case 414:
            text = "URI Too Long";
            break;
        case 500:
            text = "Internal Server Error";
            break;
        case 505:
            text = "HTTP Version Not Supported";
    }
    return text;
}

std::string getMimeType(const std::string& path)
{
    std::size_t pos = path.find_last_of(".");
    if (pos == std::string::npos)
        return "text/plain";
    std::string extension = path.substr(pos + 1);
    if (extension == "png")
        return "image/png";
    if (extension == "apng")
        return "image/apng";
    if (extension == "avif")
        return "image/avif";
    if (extension == "gif")
        return "image/gif";
    if (extension == "jpg" || extension == "jpeg")
        return "image/jpeg";
    if (extension == "svg")
        return "image/svg+xml";
    if (extension == "webp")
        return "image/webp";
    if (extension == "css")
        return "text/css";
    if (extension == "csv")
        return "text/csv";
    if (extension == "aac")
        return "audio/aac";
    if (extension == "arc")
        return "application/x-freearc";
    if (extension == "avi")
        return "video/x-msvideo";
    if (extension == "bz")
        return "application/x-bzip";
    if (extension == "bz2")
        return "application/x-bzip2";
    if (extension == "cda")
        return "application/x-cdf";
    if (extension == "csh")
        return "application/x-csh";
    if (extension == "gz")  
        return "application/gzip";
    if (extension == "html" || extension == "htm")
        return "text/html";
    if (extension == "ico")
        return "image/vnd.microsoft.icon";
    if (extension == "ics")
        return "text/calendar";
    if (extension == "jar")
        return "application/java-archive";
    if (extension == "js")
        return "text/javascript";
    if (extension == "json")
        return "application/json";
    if (extension == "md")
        return "text/markdown";
    if (extension == "mid" || extension == "midi")
        return "audio/midi";
    if (extension == "mjs")
        return "text/javascript";
    if (extension == "mp3")
        return "audio/mpeg";
    if (extension == "mp4")
        return "video/mp4";
    if (extension == "mpeg" || extension == "mpg")
        return "video/mpeg";
    if (extension == "oga" || extension == "ogg")
        return "audio/ogg";
    if (extension == "ogv")
        return "video/ogg";
    if (extension == "ogx")
        return "application/ogg";
    if (extension == "otf")
        return "font/otf";
    if (extension == "pdf")
        return "application/pdf";
    if (extension == "tar")
        return "application/x-tar";
    if (extension == "tif" || extension == "tiff")
        return "image/tiff";
    if (extension == "ts")
        return "video/mp2t";
    if (extension == "txt")
        return "text/plain";
    if (extension == "weba")
        return "audio/webm";
    if (extension == "webm")
        return "video/webm";
    if (extension == "xhtml")
        return "application/xhtml+xml";
    if (extension == "xml")
        return "application/xml";
    if (extension == "zip")
        return "application/zip";
    if (extension == "3gp")
        return "video/3gpp";
    if (extension == "3g2")
        return "video/3gpp2";
    if (extension == "7z")
        return "application/x-7z-compressed";
    if (extension == "woff")
        return "font/woff";
    if (extension == "woff2")
        return "font/woff2";
    if (extension == "xls")
        return "application/vnd.ms-excel";
    if (extension == "xul")
        return "application/vnd.mozilla.xul+xml";
    return "application/octet-stream";
}

void    parsedRequest(Request req)
{
    std::cout << "========================================" << std::endl;
    std::cout << "=============Parsed Request=============" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    std::cout << "Method: " << req.getMethod() << std::endl;
    std::cout << "Path: " << req.getPath() << std::endl;
    std::cout << "Protocol: " << req.getProtocol() << std::endl;
    std::cout << std::endl;

    std::cout << "================Headers================" << std::endl;
    std::map<std::string, std::string> headers = req.getHeaders();
    std::map<std::string, std::string>::iterator it = headers.begin();
    for (; it != headers.end(); it++)
    {
        std::cout << it->first << "=  " << it->second << std::endl;
    }
}