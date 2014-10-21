#ifndef __SGVLOG__
#define __SGVLOG__

#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <map>
#include <stdexcept>
#include <memory>
#include <cstdarg>

#include "boost/shared_ptr.hpp"
#include "boost/utility.hpp"
#include "boost/config.hpp"

#if (defined BOOST_WINDOWS) && !(defined BOOST_DISABLE_WIN32)
	#include <windows.h> 
	#include <direct.h>
#else
	#include <unistd.h>
	#include <sys/stat.h>
#endif

using namespace std;
using namespace boost;

namespace Sgv{
	
using namespace std;


#if (defined BOOST_WINDOWS) && !(defined BOOST_DISABLE_WIN32)

inline void sleep(const long& msec)
{
	::Sleep(static_cast<DWORD>(msec));
};

inline const int makeDir(const char* file)
{
	return static_cast<int>(::_mkdir(file));
};

inline const int removeDir(const char* file)
{
	return static_cast<int>(::_rmdir(file));
};

#else

inline void sleep(const long& msec)
{
	::usleep(static_cast<useconds_t>(msec*1000));
};

inline const int makeDir(const char* file)
{
	return static_cast<int>(::mkdir(file, 0));
};

inline const int removeDir(const char* file)
{
	return static_cast<int>(::rmdir(file));
};

#endif


//
class LogFileWriter;
//
typedef boost::shared_ptr<LogFileWriter> LogFileWriterPtr;

//
class Log;
typedef boost::shared_ptr<Log> LogPtr;


/**
@brief class for file roc by directory
name of directory is [file path] + "__lock_"
*/
class FileLock : noncopyable
{
public:
	/**
	@param file [in] file path to be locked
	*/
	FileLock(const string& file)
		: m_filepath(file), m_folderpath(file + string("__lock_")), m_isLocked(false), m_sleepMsec(200)
	{};

	~FileLock(){};

	/**
	Lock
	@return true if succeeded
	*/
	const bool lock();

	/**
	 Unlock
	 @return true if succeeded
	 */
	const bool unlock();

private:
	string m_filepath;   // Log file name
	string m_folderpath; // Folder name for the log file
	bool m_isLocked;     // Flag whether this object locks the file?
	long m_sleepMsec;    // Time [msec] to try again  to lock
};


/**
@brief Basic class for file writing
*/
class LogFileWriter : noncopyable
{
public:
	virtual ~LogFileWriter(){};

	/**
	 Save message
	 Some derived classes may write on the log file directly,
     other may store in the memory and write them when flush() is called
	 @param msg [in] string to be written
	*/
	virtual void write(const char* msg)=0;

	/**
	 Flush the log
	*/
	virtual void flush()=0;

	virtual LogFileWriter* clone() const =0;

protected:
	LogFileWriter(){};
};



typedef auto_ptr<LogFileWriter> LogFileWriterAutoPtr;


/**
 @brief class for file writing
 @n     Open/close the file when write() is called
 @param TFileLock, Specify the lock class. (ex.FileLock)
*/
template <class TFileLock>
class SimpleFileWriter : public LogFileWriter
{
public:
	/**
	 Constructor
	 @param file [in] Path of the log file
	*/
	SimpleFileWriter(const string& file) : m_file(file), m_lock(file)
	{};

	~SimpleFileWriter()
	{};

	void write(const char* msg)
	{
		if(!m_lock.lock()){
			cerr << "SimpleFileWriter::write error, msg=" << msg << endl;
			return;
		}

		//
		ofstream ofs(m_file.c_str(), ios_base::app);
		ofs << msg << endl;
		
		m_lock.unlock();
		return;
	};
	//Nothing to do
	void flush(){};

	//
	LogFileWriter* clone() const
	{
		return new SimpleFileWriter<TFileLock>(m_file);
	};

private:
	string m_file;
	TFileLock m_lock;
};


/**
 Base class for DelayFileWrite
 Extract the almost all parts except template to reduce the size of exe
*/
class DelayFileWriterBase : public LogFileWriter
{
public:

	/**
	 Write on temporary file
     @param msg [in] message to be sent
	*/
	void write(const char* msg)
	{
		if(!m_isStart){
			createTmpFile();
		}
		//
		m_ofs << msg << endl;
	};


protected:
	/**
	 Constructor
	 @param file [in] path of log file
	*/
	DelayFileWriterBase(const string& file)	: m_file(file), m_isStart(false)
	{};

	///
	virtual ~DelayFileWriterBase(){};
	
	// Output to the log file. Callced from destructor
	void writeLog();

	void createTmpFile();

	// File name for temporary
	const string createFileName() const;

	const string& getFile() const
	{ return m_file;};

	inline const bool& getIsStart() const
	{ return m_isStart; };

	inline void close()
	{ 
		m_ofs.close(); 
		m_isStart = false;
	};

private:
	string m_file;
	string m_tmpfile;
	ofstream m_ofs;
	bool m_isStart;
};


/**
 @brief Log writing with delay. When the object is broken, it write on the log file
 @n     It thus faster than open/close style in every writing
 @n     If the process quit with unexpected error, the log never be written¡£
 @param TFileLock Specify the lock file (ex.FileLock)
*/
template <class TFileLock>
class DelayFileWriter : public DelayFileWriterBase
{
public:
	///
	DelayFileWriter(const string& file)
		: DelayFileWriterBase(file), m_lock(file)
	{};

	// Output to the log file when the object is destructed
	// If it failed, the temporary file should be remained without deletion
	virtual ~DelayFileWriter()
	{
		flush();
	};

	// Write the contents of temporary file, and delete the temporary file
	void flush()
	{
		// Nothing to do if the log is not started
		if(!getIsStart()) return;

		// Start of the lock
		if(!m_lock.lock()) return;
		
		// Output to the log file
		writeLog();
		close();

		// End of the lock
		m_lock.unlock();
	};

	LogFileWriter* clone() const
	{
		return new DelayFileWriter<TFileLock>(getFile());
	};

private:
	TFileLock m_lock;
};


/**
Create messages from variable argument
@param oss [in] output stream
@param src [in] source string to be converted
@param ap  [in] list of variable arguments
<pre>
%s ...output string (char*)
%d ...output int
%l ...long
%f ...double
%c ...char
</pre>
*/
void makeMsg(ostream& oss, const char* src, va_list& ap);

/**
Output of time stamp such as "2008/07/26"
*/
void writeLogHeader(ostream& os);


/**
@brief Base class for log output. describe here except template to avoid huge exe
*/
class LogBase
{
public:
	
	enum LogLevel{ERR=0, INFO, DEBUG, WARNING};

	inline const LogLevel& getLevel() const
	{ return m_level; };

	inline const string& getId() const
	{ return m_id; };

protected:
	/**
	@param level [in] Log level
	@param id    [in] ID string (program name, display name and so on)
	*/	
	LogBase(const enum LogLevel level, const string& id)
		: m_level(level), m_id(id)
	{};

	~LogBase(){};

	/**
	Output of the log regardless the log level
	*/
	void write(LogFileWriter& writer, const char* level, const char* msg)
	{
		ostringstream oss;
		writeLogHeader(oss);
		oss << '[' << level << "] " << '[' << m_id << "] " << msg;
		
		writer.write(oss.str().c_str());
	};

private:
	LogLevel m_level;
	// ID string (program name, display name and so on)
	string m_id;
};


/**
Return the log level as string
@param level [in]
 */
const char* logLevelToStr(const LogBase::LogLevel level);


/**
Return the log level value from string. If target is not found, INFO should be returned
@param levelStr [in]
*/
LogBase::LogLevel StrToLogLevel(const string& levelStr);


/**
Use template specialization or define operator << <Obj>(ostream&, const Obj) <br>
If template specialization is not used, os << obj; will be executed
Used by Log::printObj()
*/
template <class Obj>
void toString(ostream& os, const Obj& obj)
{
	os << obj;
};



/**
@brief Log output class
<pre>
** Usage **
Log<DelayFileWriter<FileLock> > log("testlog.txt", Log::INFO, "main");

log.err("This is error");        // It should be output
log.info("This is information"); // It should be output
log.warning("This is warning");  // It should not be output. If Log::WARNING is used as level, it should be output

//printf-wise output is available. Refer the format at Log::printf()
//log.printf(Log::INFO, "%d-th time comment '%s' came", 12, "message of SIGVerse");


** Extension **
If you want to output in other ways, refer the following methods
- In a case that file lock method should be changed
   Create file lock class following FileLock class
   The requied methods in file lock class are the followings:
    - Constructer which take an argument for file path string (const string& file)
	¡¦const bool lock();
	¡¦const bool unlock();

- In a case that output method should be changed
   Create file lock class following SimpleFileWriter class
   The requied methods in file lock class are the followings:
    - Constructer which take an argument for file path string (const string& file)
    - const bool write(const char* msg) which writes on a file
	- template class should be used. File lock class should be managed by member variables
</pre>
*/
class Log : public LogBase
{
public:
	/**
	Constructer
	@param file  [in] file path
	@param level [in] log level
	@param id    [in] ID
	*/
	Log(const enum LogLevel level, const string& id, auto_ptr<LogFileWriter> writer)
		: LogBase(level, id), m_writer(writer)
	{};

	~Log(){};
	
	LogPtr clone() const
	{
		return LogPtr(
			new Log(
				getLevel(), getId(), auto_ptr<LogFileWriter>(m_writer->clone())
			)
		);
	};

	inline void info(const char* msg)
	{
		if(getLevel() < INFO) return;
		write(*m_writer, logLevelToStr(INFO), msg);
	};

	inline void debug(const char* msg)
	{
		if(getLevel() < DEBUG) return;
		write(*m_writer, logLevelToStr(DEBUG), msg);
	};

	inline void err(const char* msg)
	{
		if(getLevel() < ERR) return;
		write(*m_writer, logLevelToStr(ERR), msg);
	};
	
	inline void warning(const char* msg)
	{
		if(getLevel() < WARNING) return;
		write(*m_writer, logLevelToStr(WARNING), msg);
	};

	/**
	Log output by variable arguments. Refer the format in makeMsg()
	This method is no to fast. Not so recommended to use <br>
	If different number of arguments is given, it may occur serious problems
	*/
	void printf(const LogLevel level, const char* msg, ...)
	{
		if(getLevel() < level) return;

		va_list ap;
		va_start(ap, msg);
		ostringstream oss;
		makeMsg(oss, msg, ap);
		write(*m_writer, logLevelToStr(level), oss.str().c_str());
		va_end(ap);
	};

	/**
	Output template <class Obj> void toString(ostream& os, const Obj& obj) after msg
	*/
	template <class Object>
	void printObj(const LogLevel level, const char* msg, const Object& obj)
	{
		if(getLevel() < level) return;
		
		ostringstream oss;
		oss << msg << " ";
		toString<Object>(oss, obj);
		
		write(*m_writer, logLevelToStr(level), oss.str().c_str());
	};

	/**
	Flush the log. Delay sometimes occurs depends on type of LogFileWriter<br>
	Output is executed when destructor is called, but estimation is difficult when the destructor is called<br>
	Thus, this method should be used to output any favority timing
	*/	
	inline void flush()
	{
		m_writer->flush();
	};

protected:

private:
	// Log output class
	auto_ptr<LogFileWriter> m_writer;
};



/**
@brief Log factory
<pre>
It is possible to refer different objects from each ID
It is better to use multithread in near future
It is better to enable each thread to get the same object
Currently, the same object is return if the ID is the same
</pre>
*/
class LogFactory
{
	typedef std::map<int, boost::shared_ptr<Log> > LogMap;
public:
	/**
	@exception When runtime_error id is not found
	*/
	static LogPtr getLog(const int id)
	{
		LogMap::iterator i = m_logMap.find(id);
		if(i == m_logMap.end()) throw runtime_error("the id not exists. error in Logfactory::getLog()");
		return i->second;
	};

	static void setLog(const int id, const LogPtr& log)
	{
		m_logMap[id] = log;
	};

protected:
	LogFactory(){};
private:
	static LogMap m_logMap;
};


}; //namespace Sgv{

#endif


