/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/**
 * This is a simple sink for the ILog.h logging API.
 * It routes all logging messages to the console (stdout & stderr).
 */
#include "ConsoleSink.h"

#include "Backend.h"
#include "FramePrefixer.h"
#include "Level.h" // for LOG_LEVEL_*
#include "System/MainDefines.h"

#include <cstdio> // for stderr, stdout
#include <atomic>
#include <chrono>


#ifdef __cplusplus
extern "C" {
#endif

static bool colorizedOutput = false;



/**
 * @name logging_sink_console
 * ILog.h sink implementation.
 */
///@{


void log_console_colorizedOutput(bool enable) {
	colorizedOutput = enable;
}


/// Records a log entry
static void log_sink_record_console(int level, const char* section, const char* record)
{
	char framePrefix[128] = {'\0'};
	log_framePrefixer_createPrefix(framePrefix, sizeof(framePrefix));

	FILE* outStream = stdout;

	const char* fstr = "%s%s\n";
	if (colorizedOutput) {
		if (level >= LOG_LEVEL_ERROR) {
			fstr = "\033[90m%s\033[31m%s\033[39m\n";
		} else if (level == LOG_LEVEL_DEPRECATED) {
			fstr = "\033[90m%s\033[34m%s\033[39m\n";
		} else if (level >= LOG_LEVEL_WARNING) {
			fstr = "\033[90m%s\033[33m%s\033[39m\n";
		} else {
			fstr = "\033[90m%s\033[39m%s\n";
		}
	}

	FPRINTF(outStream, fstr, framePrefix, record);

	// supcom: flush on errors and at most every 250 ms instead of after every line. A console write per line cost about
	// 25 ms on Windows (2,474 lines took 62 s in a headless run); stdout is fully buffered (ConsoleSinkRegistrator) and
	// flushed at exit, and the infolog file is written by its own sink. Atomic: several threads log.
	static std::atomic<long long> lastFlushMs{0};
	const long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();

	if (level >= LOG_LEVEL_ERROR || (nowMs - lastFlushMs.load(std::memory_order_relaxed)) >= 250) {
		fflush(outStream);
		lastFlushMs.store(nowMs, std::memory_order_relaxed);
	}
}

///@}

#ifdef __cplusplus
} // extern "C"
#endif

ConsoleSinkRegistrator::ConsoleSinkRegistrator() {
	// supcom: a full buffer, flushed by the sink (see log_sink_record_console)
	setvbuf(stdout, nullptr, _IOFBF, 1 << 16);
	log_backend_registerSink(&log_sink_record_console);
}
ConsoleSinkRegistrator::~ConsoleSinkRegistrator() {
	log_backend_unregisterSink(&log_sink_record_console);
	fflush(stdout);
}
