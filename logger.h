#ifndef __TCP_PROXY_LOGGER
#define __TCP_PROXY_LOGGER

#include <pthread.h>
#include <string>
#include <sstream>
#include <iomanip>

class Logger
{
public:
    enum Level
    {
        LV_DEBUG,
        LV_INFO,
        LV_WARNING,
        LV_ERROR,
        LV_FATAL
    };
    
public:
    Logger();
    ~Logger();
    
    void setPath(const std::string& strPath, const std::string& strModule);
    void setLevel(int nLevel);

    // DEBUG,INFO,WARNING,ERROR
    void setLevel(const std::string& strLevel);
    int  getLevel() const { return m_nLevel; }
    void setConsole(bool bOn);
    void write(int nLevel, const std::string& data);
    
    static std::ostream& genHeader(std::ostream& os, int nLevel, const char* file, int line);
    static const char* shortFileName(const char* strFullFile);

    static Logger m_nLogger;
    static Logger& instance() { return m_nLogger; }
    
private:
    void openFile();
    void closeFile();
    
    std::string m_strPath;
    std::string m_strModule;
    int    m_nLevel;
    bool   m_bConsoleOn;
    FILE*  m_fp;
    pthread_mutex_t m_nMutex;
    int    m_nLastDay;
};

//
// LogMessage
//
class LogMessage
{
public:
    LogMessage(const char* file, int line, 
        Logger& logger, int nLevel) : m_logger(logger), _logLevel(nLevel)
    {
        Logger::genHeader(_os, nLevel, file, line);
    }
    ~LogMessage()
    {
        m_logger.write(_logLevel, _os.str());
        if (_logLevel == Logger::LV_FATAL) exit(-1);
    }
    
    std::ostream& stream() { return _os; }
private:
    Logger& m_logger;
    int     _logLevel;
    std::ostringstream _os;
};

//
// LogMessageIgnoreStream
//
class LogMessageIgnoreStream
{
public:
    LogMessageIgnoreStream() {}
    void operator&(std::ostream&) {}
};

/*
日志包装工具，提供日志流的使用方式。格式化使用c++ iomanip.h 定义的格式。
使用例子如下：
    AILOG_DEBUG() << "This is log:" << 'a' << 100;
    AILOG_INFO() << "This is log:" << 'a' << 100;
    AILOG_WARN() << "This is log:" << 'a' << 100;
    AILOG_ERROR() << "This is log:" << 'a' << 100;
    AILOG_FATAL() << "This is log:" << 'a' << 100;
    AILOG_FATAL() << "This is log:" << std::setprecision(2) << 1/3;
    ...
*/
#ifndef AILOG_LAZY_STREAM_BASE
#define AILOG_LAZY_STREAM_BASE(cond, stream)\
    !(cond) ? (void)0 : LogMessageIgnoreStream() & stream
#endif

#ifndef AILOG_VLOG_BASE
#define AILOG_VLOG_BASE(logger, level)\
    AILOG_LAZY_STREAM_BASE(logger.getLevel() <= level, LogMessage(__FILE__, __LINE__, logger, level).stream())

#endif

#ifndef AILOG_DEBUG_TO
#define AILOG_DEBUG_TO(logger) AILOG_VLOG_BASE(logger, Logger::LV_DEBUG)
#endif
#ifndef AILOG_INFO_TO
#define AILOG_INFO_TO(logger)  AILOG_VLOG_BASE(logger, Logger::LV_INFO)
#endif
#ifndef AILOG_WARN_TO
#define AILOG_WARN_TO(logger)  AILOG_VLOG_BASE(logger, Logger::LV_WARNING)
#endif 
#ifndef AILOG_ERROR_TO
#define AILOG_ERROR_TO(logger) AILOG_VLOG_BASE(logger, Logger::LV_ERROR)
#endif
#ifndef AILOG_FATAL_TO
#define AILOG_FATAL_TO(logger) AILOG_VLOG_BASE(logger, Logger::LV_FATAL)     
#endif

#ifndef AILOG_DEBUG
#define AILOG_DEBUG() AILOG_VLOG_BASE(Logger::m_nLogger, Logger::LV_DEBUG)
#endif

#ifndef AILOG_INFO
#define AILOG_INFO()  AILOG_VLOG_BASE(Logger::m_nLogger, Logger::LV_INFO)
#endif

#ifndef AILOG_WARN
#define AILOG_WARN()  AILOG_VLOG_BASE(Logger::m_nLogger, Logger::LV_WARNING)
#endif

#ifndef AILOG_ERROR
#define AILOG_ERROR() AILOG_VLOG_BASE(Logger::m_nLogger, Logger::LV_ERROR)
#endif

#ifndef AILOG_FATAL
#define AILOG_FATAL() AILOG_VLOG_BASE(Logger::m_nLogger, Logger::LV_FATAL)
#endif

#endif
