/*******************************************************************************
#                                                                              #
# This file is part of mp710Ctrl.                                              #
#                                                                              #
# Copyright (C) 2015 Oleg Efremov                                              #
#                                                                              #
# mp710Ctrl is free software; you can redistribute it and/or modify            #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
*******************************************************************************/

#include "Tracer.h"

#include <cstdio>
#include <cstdarg> 
#include <cstring>
#include <syslog.h>
#include <errno.h>

namespace Tracer {
  static const char* TraceName = "UvcStreamer"; 
  
  namespace {
    bool IsStdErrReady() {
      return fileno(stderr) != -1;
    }
    
    void LogVArgs(const char* format, va_list args) {
      
      if (IsStdErrReady()) {
        std::vfprintf(stderr, format, args);
      }
      else {
        static bool isSysLogInitialized = false;
        if (!isSysLogInitialized) {
          ::openlog(Tracer::TraceName, LOG_ODELAY, LOG_USER | LOG_ERR);
          isSysLogInitialized = true;
        }

        ::vsyslog(LOG_ERR, format, args);
      }
    }
  }


  void Log(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LogVArgs(format, args);
    va_end(args);
  }
  
  void LogErrNo(const char* format, ...) {
    va_list args;
    va_start(args, format);
    LogVArgs(format, args);
    va_end(args);
    
    char errorMessageBuff[128] = {0};
    
    char* errorMessage = strerror_r(errno, errorMessageBuff, sizeof(errorMessageBuff));
    if (errorMessage != nullptr) {
      Log("%s\n", errorMessage);
    }
  }
}
