
#include "logger.h"
#include <pthread.h>
#include <unistd.h>
#include <cstdio>
#include <ctime>

Logger Logger::m_nLogger;

Logger::Logger()
    : m_strPath("."), m_strModule("app"), 
      m_nLevel(LV_INFO), m_bConsoleOn(true), 
      m_fp(NULL), m_nLastDay(0)
{
    pthread_mutex_init(&m_nMutex, NULL);
}

Logger::~Logger()
{
    closeFile();
    pthread_mutex_destroy(&m_nMutex);
}

void Logger::setPath(const std::string& strPath, const std::string& strModule)
{
    m_strPath = strPath;
    m_strModule = strModule;
}

void Logger::setLevel(int nLevel)
{
    m_nLevel = (nLevel >= LV_DEBUG && nLevel <= LV_ERROR) ? nLevel : m_nLevel; 
}

void Logger::setLevel(const std::string& strLevel)
{
    static const char* str[] = {"DEBUG", "INFO", "WARNING", "ERROR"};
    for (int i = 0; i < 4; ++i)
    {
        if (strLevel == str[i])
        {
            m_nLevel = i;
            return;
        }
    }
}

void Logger::setConsole(bool bOn)
{
    m_bConsoleOn = bOn;
}

void Logger::write(int nLevel, const std::string& data)
{
    pthread_mutex_lock(&m_nMutex);
    time_t ts = time(NULL);
    struct tm gtm = *localtime(&ts);
    if (gtm.tm_mday != m_nLastDay)
    {
        closeFile();
        openFile();
    }
    
    if (m_fp)
    {
        fprintf(m_fp, "%s\n", data.c_str());
        fflush(m_fp);
        if (m_bConsoleOn)
        {
            fprintf(stdout, "%s\n", data.c_str());
            fflush(m_fp);
        }
    }
    pthread_mutex_unlock(&m_nMutex);
}

const char* Logger::shortFileName(const char* strFullFile)
{
    const char* p = strFullFile + strlen(strFullFile);
    while (p > strFullFile && *p != '/') --p;
    return p == strFullFile ? p : (p + 1);
}

std::ostream& Logger::genHeader(std::ostream& os, int nLevel, const char* file, int line)
{
    static const char* gLevelStr[] = {"DBG", "INF", "WAR", "ERR", "FTL"};
    time_t ts = time(NULL);
    struct tm gtm = *localtime(&ts);
    char buffer[16];
    #if defined(WIN32)
    DWORD pid = GetCurrentThreadId();
    #else
    pthread_t pid = pthread_self();
    #endif
    sprintf(buffer, "%02d:%02d:%02d", gtm.tm_hour, gtm.tm_min, gtm.tm_sec);
    return os << "[" << pid << ":" << getpid() << " " 
        << (const char*)buffer << " " << gLevelStr[nLevel] << " " 
        << shortFileName(file) << ":" << line << "] ";
}

void Logger::openFile()
{
    time_t ts = time(NULL);
    struct tm gtm = *localtime(&ts);
    
    std::ostringstream os;
    os << m_strPath << "/" << m_strModule << ".log." 
        << std::setfill('0') << std::setw(4) << (1900 + gtm.tm_year)
        << std::setfill('0') << std::setw(2) << (gtm.tm_mon + 1)
        << std::setfill('0') << std::setw(2) << gtm.tm_mday;

    m_fp = fopen(os.str().c_str(), "w");
    if (!m_fp)
    {
        fprintf(stderr, "open Logger file(%s) error\n", os.str().c_str());
    }
    m_nLastDay = gtm.tm_mday;
}

void Logger::closeFile()
{
    if (m_fp)
    {
        fclose(m_fp);
        m_fp = NULL;
    }
}
