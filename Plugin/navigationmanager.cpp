//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : navigationmanager.cpp
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
#include "navigationmanager.h"

#include "codelite_events.h"
#include "event_notifier.h"
#include "ieditor.h"
#include "imanager.h"

#include <wx/stc/stc.h>

NavMgr::NavMgr() { EventNotifier::Get()->Bind(wxEVT_WORKSPACE_CLOSED, &NavMgr::OnWorkspaceClosed, this); }

NavMgr::~NavMgr()
{
    EventNotifier::Get()->Unbind(wxEVT_WORKSPACE_CLOSED, &NavMgr::OnWorkspaceClosed, this);
    Clear();
}

NavMgr* NavMgr::Get()
{
    static NavMgr theManager;
    return &theManager;
}

void NavMgr::Clear()
{
    m_prevs = {};
    m_nexts = {};
}

bool NavMgr::ValidLocation(const BrowseRecord& rec) const { return (!rec.filename.IsEmpty()) && (rec.lineno > 1); }

bool NavMgr::CanNext() const { return !m_nexts.empty(); }

bool NavMgr::CanPrev() const { return !m_prevs.empty(); }

void NavMgr::StoreCurrentLocation(const BrowseRecord& origin, const BrowseRecord& target)
{
    clDEBUG() << "Nav manager storing location: Origin:" << origin << ", Target:" << target << endl;
    if (m_prevs.empty() || !m_prevs.top().IsSameAs(origin)) {
        m_prevs.push(origin);
    }
    m_nexts = {}; // diverging from the navigation we reset the "forward" action
    m_currentLocation = target;
}

bool NavMgr::NavigateBackward(IManager* mgr)
{
    if (!CanPrev()) {
        return false;
    }

    auto new_loc = m_prevs.top();
    m_prevs.pop();
    if (m_currentLocation.IsOk()) {
        m_nexts.push(m_currentLocation);
    }
    m_currentLocation = new_loc;

    clDEBUG() << "Nav manager BACKWARD:" << new_loc << endl;
    auto callback = [=](IEditor* editor) {
        editor->GetCtrl()->ClearSelections();
        editor->CenterLine(new_loc.lineno, new_loc.column);
    };
    mgr->OpenFileAndAsyncExecute(new_loc.filename, std::move(callback));
    return true;
}

bool NavMgr::NavigateForward(IManager* mgr)
{
    auto next_loc = GetNextLocation();
    if (!next_loc.IsOk()) {
        return false;
    }

    clDEBUG() << "Nav manager FORWARD:" << next_loc << endl;
    auto callback = [=](IEditor* editor) mutable {
        editor->GetCtrl()->ClearSelections();
        editor->CenterLine(next_loc.lineno, next_loc.column);
    };
    mgr->OpenFileAndAsyncExecute(next_loc.filename, std::move(callback));
    return true;
}

void NavMgr::OnWorkspaceClosed(clWorkspaceEvent& e)
{
    e.Skip();
    Clear();
}

BrowseRecord NavMgr::GetNextLocation()
{
    while (!m_nexts.empty()) {
        auto new_loc = m_nexts.top();
        m_nexts.pop();

        if (m_currentLocation.IsOk() && m_currentLocation.IsSameAs(new_loc)) {
            // Choose another location
            continue;
        } else if (m_currentLocation.IsOk()) {
            m_prevs.push(m_currentLocation);
        }

        m_currentLocation = new_loc;
        return new_loc;
    }

    return {};
}
