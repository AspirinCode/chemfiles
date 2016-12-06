// Chemfiles, a modern library for chemistry file reading and writing
// Copyright (C) 2015-2016 Guillaume Fraux and contributors
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/

#ifndef CHEMFILES_CAPI_ERRORS_H
#define CHEMFILES_CAPI_ERRORS_H

#include "chemfiles.h"

#include "chemfiles/Error.hpp"
#include "chemfiles/warnings.hpp"

#include <map>
#include <string>
#include <cassert>
#include <cstdint>

namespace chemfiles {
    extern std::string CAPI_LAST_ERROR;
}

inline size_t checked_cast(uint64_t value) {
    if (static_cast<uint64_t>(value) > static_cast<uint64_t>(SIZE_MAX)) {
        throw chemfiles::Error("Got a value too big to be represented by a size_t on this system");
    }
    return static_cast<size_t>(value);
}

#define CATCH_AND_RETURN(exception, retval)                                    \
    catch (const chemfiles::exception& e) {                                    \
        CAPI_LAST_ERROR = std::string(e.what());                               \
        chemfiles::warning(e.what());                                          \
        return retval;                                                         \
    }

#define CATCH_AND_GOTO(exception)                                              \
    catch (const chemfiles::exception& e) {                                    \
        CAPI_LAST_ERROR = std::string(e.what());                               \
        chemfiles::warning(e.what());                                          \
        goto error;                                                            \
    }

/// Wrap `instructions` in a try/catch bloc automatically, and return a status
/// code
#define CHFL_ERROR_CATCH(instructions)                                         \
    try {                                                                      \
        instructions                                                           \
    }                                                                          \
    CATCH_AND_RETURN(FileError, CHFL_FILE_ERROR)                               \
    CATCH_AND_RETURN(MemoryError, CHFL_MEMORY_ERROR)                           \
    CATCH_AND_RETURN(FormatError, CHFL_FORMAT_ERROR)                           \
    CATCH_AND_RETURN(SelectionError, CHFL_SELECTION_ERROR)                     \
    CATCH_AND_RETURN(Error, CHFL_GENERIC_ERROR)                                \
    catch (const std::exception& e) {                                          \
        CAPI_LAST_ERROR = std::string(e.what());                               \
        return CHFL_CXX_ERROR;                                                 \
    }                                                                          \
    return CHFL_SUCCESS;

/// Wrap `instructions` in a try/catch bloc automatically, and goto the
/// `error` label in case of error.
#define CHFL_ERROR_GOTO(instructions)                                          \
    try {                                                                      \
        instructions                                                           \
    }                                                                          \
    CATCH_AND_GOTO(FileError)                                                  \
    CATCH_AND_GOTO(MemoryError)                                                \
    CATCH_AND_GOTO(FormatError)                                                \
    CATCH_AND_GOTO(SelectionError)                                             \
    CATCH_AND_GOTO(Error)                                                      \
    catch (const std::exception& e) {                                          \
        CAPI_LAST_ERROR = std::string(e.what());                               \
        goto error;                                                            \
    }

#endif
