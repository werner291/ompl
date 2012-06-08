/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2008, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

/* Author: Ioan Sucan */

#include "ompl/util/Console.h"
#include <boost/thread/mutex.hpp>
#include <boost/filesystem/path.hpp>
#include <iostream>
#include <cstdio>
#include <cstdarg>

/// @cond IGNORE

struct DefaultOutputHandler
{
    DefaultOutputHandler(void)
    {
        output_handler_ = static_cast<ompl::msg::OutputHandler*>(&std_output_handler_);
        previous_output_handler_ = output_handler_;
        logLevel_ = ompl::msg::DEBUG;
        showLineNumbers_ = false;
    }

    ompl::msg::OutputHandlerSTD std_output_handler_;
    ompl::msg::OutputHandler   *output_handler_;
    ompl::msg::OutputHandler   *previous_output_handler_;
    ompl::msg::LogLevel         logLevel_;
    boost::mutex                lock_; // it is likely the outputhandler does some I/O, so we serialize it
    bool                        showLineNumbers_;
};

// we use this function because we want to handle static initialization correctly
// however, the first run of this function is not thread safe, due to the use of a static
// variable inside the function. For this reason, we ensure the first call happens during
// static initialization using a proxy class
static DefaultOutputHandler* getDOH(void)
{
    static DefaultOutputHandler DOH;
    return &DOH;
}

#define USE_DOH                                                                \
    DefaultOutputHandler *doh = getDOH();                                      \
    boost::mutex::scoped_lock slock(doh->lock_)

#define MAX_BUFFER_SIZE 1024

/// @endcond

void ompl::msg::noOutputHandler(void)
{
    USE_DOH;
    doh->previous_output_handler_ = doh->output_handler_;
    doh->output_handler_ = NULL;
}

void ompl::msg::restorePreviousOutputHandler(void)
{
    USE_DOH;
    std::swap(doh->previous_output_handler_, doh->output_handler_);
}

void ompl::msg::useOutputHandler(OutputHandler *oh)
{
    USE_DOH;
    doh->previous_output_handler_ = doh->output_handler_;
    doh->output_handler_ = oh;
}

ompl::msg::OutputHandler* ompl::msg::getOutputHandler(void)
{
    return getDOH()->output_handler_;
}

void ompl::msg::log(const char *file, int line, LogLevel level, const char* m, ...)
{
    USE_DOH;
    if (doh->output_handler_ && level >= doh->logLevel_)
    {
        va_list __ap;
        va_start(__ap, m);
        char buf[MAX_BUFFER_SIZE];
        vsnprintf(buf, sizeof(buf), m, __ap);
        va_end(__ap);
        buf[MAX_BUFFER_SIZE - 1] = '\0';

        std::stringstream ss;
        if (doh->showLineNumbers_)
            ss << "line " << line << " in " << boost::filesystem::path(file).filename().string() << ": ";
        ss << buf;

        switch (level)
        {
            case ERROR:
                doh->output_handler_->error (ss.str());
                break;

            case WARN:
                doh->output_handler_->warn (ss.str());
                break;

            case INFO:
                doh->output_handler_->inform (ss.str());
                break;

            case DEBUG:
                doh->output_handler_->debug (ss.str());
                break;

            case NONE: // intentional fall through.  These cases should never happen
            default:
                doh->output_handler_->error (ss.str());
                break;
        }
    }
}

void ompl::msg::setLogLevel(LogLevel level)
{
    USE_DOH;
    doh->logLevel_ = level;
}

ompl::msg::LogLevel ompl::msg::getLogLevel(void)
{
    USE_DOH;
    return doh->logLevel_;
}

void ompl::msg::showLineNumbers(bool show)
{
    USE_DOH;
    doh->showLineNumbers_ = show;
}

void ompl::msg::OutputHandlerSTD::error(const std::string &text)
{
    std::cerr << "Error:   " << text << std::endl;
    std::cerr.flush();
}

void ompl::msg::OutputHandlerSTD::warn(const std::string &text)
{
    std::cerr << "Warning: " << text << std::endl;
    std::cerr.flush();
}

void ompl::msg::OutputHandlerSTD::inform(const std::string &text)
{
    std::cout << "Info:    " << text << std::endl;
    std::cout.flush();
}

void ompl::msg::OutputHandlerSTD::debug(const std::string &text)
{
    std::cout << "Debug:   " << text << std::endl;
    std::cout.flush();
}

ompl::msg::OutputHandlerFile::OutputHandlerFile(const char *filename) : OutputHandler()
{
    file_ = fopen(filename, "a");
    if (!file_)
        std::cerr << "Unable to open log file: '" << filename << "'" << std::endl;
}

ompl::msg::OutputHandlerFile::~OutputHandlerFile(void)
{
    if (file_)
        if (fclose(file_) != 0)
            std::cerr << "Error closing logfile" << std::endl;
}

void ompl::msg::OutputHandlerFile::error(const std::string &text)
{
    if (file_)
    {
        fprintf(file_, "Error:   %s\n", text.c_str());
        fflush(file_);
    }
}

void ompl::msg::OutputHandlerFile::warn(const std::string &text)
{
    if (file_)
    {
        fprintf(file_, "Warning: %s\n", text.c_str());
        fflush(file_);
    }
}

void ompl::msg::OutputHandlerFile::inform(const std::string &text)
{
    if (file_)
    {
        fprintf(file_, "Info:    %s\n", text.c_str());
        fflush(file_);
    }
}

void ompl::msg::OutputHandlerFile::debug(const std::string &text)
{
    if (file_)
    {
        fprintf(file_, "Debug:   %s\n", text.c_str());
        fflush(file_);
    }
}
