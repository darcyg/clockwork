/*
  Copyright (C) 2012 Martin Leadbeater, Michael O'Connor

  This file is part of Latproc

  Latproc is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.
  
  Latproc is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Latproc; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <string>
#include <sstream>
#include <iostream>
#include <sys/time.h>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <stdio.h>
#include "Logger.h"
#include "boost/thread/mutex.hpp"

LogState* LogState::state_instance = 0;
static std::string header;
Logger *Logger::logger_instance = 0;
struct timeval last_time;

int Logger::None;
int Logger::Debug;
int Logger::Important;
int Logger::Everything;

Logger::Logger() : log_level(None), dummy_output(0), log_stream(&std::cout) {
	last_time.tv_sec = 0;
	last_time.tv_usec = 0;
	None = LogState::instance()->insert(LogState::instance()->define("None"));
	Important = LogState::instance()->insert(LogState::instance()->define("Important"));
	Debug = LogState::instance()->insert(LogState::instance()->define("Debug"));
	Everything = LogState::instance()->insert(LogState::instance()->define("Everything"));
}

void Logger::setLevel(std::string level_name){
    if(level_name=="None")setLevel(None);
    else if(level_name=="Important")setLevel(Important);
    else if(level_name=="Debug")setLevel(Debug);
    else if(level_name=="Everything")setLevel(Everything);
}

std::ostream&Logger::log(Level l){
    boost::mutex::scoped_lock lock(mutex_);
	if (!dummy_output) dummy_output = new std::stringstream;
	
    if(LogState::instance()->includes(l)){
        time_t now=time(0);
        struct tm now_tm;
  	    localtime_r(&now,&now_tm);
		struct timeval now_tv;
		gettimeofday(&now_tv,0);
		char buf[50];
        uint32_t msec = now_tv.tv_usec / 1000L;
		sprintf(buf,"%04d-%02d-%02d %02d:%02d:%02d.%03d ",
			now_tm.tm_year+1900, now_tm.tm_mon+1, now_tm.tm_mday,
			now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec, msec);
       
#if 0
        ss<<std::setfill('0')<<(now_tm.tm_year+1900)
        <<'-'<<std::setw(2)<<(now_tm.tm_mon+1)<<"-"
        <<std::setw(2)<<(now_tm.tm_mday)<<' '
        <<std::setw(2)<<now_tm.tm_hour<<':'
        <<std::setw(2)<<now_tm.tm_min<<':'
        <<std::setw(2)<<now_tm.tm_sec<<'.'
        <<std::setw(3)<<(now_tv.tv_usec / 1000)<<' ';
		header = ss.str();
		*log_stream << header;
#endif
		*log_stream << buf;
        return *log_stream;
    }
    else{
        dummy_output->clear();
        dummy_output->seekp(0);
        return*dummy_output;
    }
}
std::ostream &LogState::operator<<(std::ostream &out) const {
	std::map<std::string, int>::const_iterator iter = name_map.begin();
	const char *delim = "";
	while (iter != name_map.end()) {
		std::pair<std::string, int> const curr = *iter++;
		out << delim << curr.first <<  " " << (state_flags.count(curr.second) ? "on" : "off");
		delim = "\n";
	}
	out << std::flush;
	return out;
}

std::ostream &operator<<(std::ostream &out, const LogState&ls) {
	return ls.operator<<(out);
}

#ifdef LOGGER_DEBUG
int main(int argc,char* *argv){
    Logger::setLevel(Logger::Debug);

	std::cout << " Logger settings:\n" << LogState::instance() << "\n";
    DBG_MSG<<"this is a Debug log test\n";
    NB_MSG<<"this is an Important log test\n";
    INF_MSG<<"this is not very Important\n";
    
    return 0;
}
#endif

