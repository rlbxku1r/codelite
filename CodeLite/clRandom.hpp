//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2022 Eran Ifrah
// file name            : clRandom.hpp
//
// -------------------------------------------------------------------------
// A
//              _____           _      _     _ _
//             /  __ \         | |    | |   (_) |
//             | /  \/ ___   __| | ___| |    _| |_ ___
//             | |    / _ \ / _  |/ _ \ |   | | __/ _ )
//             | \__/\ (_) | (_| |  __/ |___| | ||  __/
//              \____/\___/ \__,_|\___\_____/_|\__\___|
//
//                                                  F i l e
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#ifndef CLRANDOM_HPP
#define CLRANDOM_HPP

#include "codelite_exports.h"

#include <cstddef>
#include <random>
#include <type_traits>
#include <wx/string.h>

class WXDLLIMPEXP_CL clRandom
{
    std::mt19937 m_engine;

public:
    template <typename T> T Generate()
    {
        static_assert(std::is_integral<T>(), "T must be an integral type");
        return static_cast<T>(m_engine());
    }
    wxString Generate(std::size_t length,
                      const wxString& candidates = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");

    static clRandom& Get();

private:
    clRandom();
    ~clRandom();
};

#endif // CLRANDOM_HPP
