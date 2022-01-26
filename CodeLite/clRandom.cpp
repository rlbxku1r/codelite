//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2022 Eran Ifrah
// file name            : clRandom.cpp
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

#include "clRandom.hpp"

#include <ctime>

clRandom::clRandom()
    : m_engine(std::time(nullptr))
{
}

clRandom::~clRandom() {}

wxString clRandom::Generate(std::size_t length, const wxString& candidates)
{
    if(candidates.IsEmpty()) {
        return {};
    }
    wxString s;
    for(std::size_t i = 0; i < length; ++i) {
        std::size_t rnd = Generate<std::size_t>();
        std::size_t at = rnd % candidates.size();
        s << candidates[at];
    }
    return s;
}

clRandom& clRandom::Get()
{
    static clRandom instance;
    return instance;
}
