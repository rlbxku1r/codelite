//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//
// copyright            : (C) 2008 by Eran Ifrah
// file name            : globals.cpp
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
#include "globals.h"

#include "AsyncProcess/asyncprocess.h"
#include "Console/clConsoleBase.h"
#include "Debugger/debuggermanager.h"
#include "FileSystemWorkspace/clFileSystemWorkspace.hpp"
#include "SelectFileTypesDialog.hpp"
#include "StringUtils.h"
#include "clDataViewListCtrl.h"
#include "clGetTextFromUserDialog.h"
#include "clTreeCtrl.h"
#include "cl_standard_paths.h"
#include "debugger.h"
#include "dirtraverser.h"
#include "drawingutils.h"
#include "editor_config.h"
#include "event_notifier.h"
#include "file_logger.h"
#include "fileutils.h"
#include "ieditor.h"
#include "imanager.h"
#include "macros.h"
#include "md5/wxmd5.h"
#include "precompiled_header.h"
#include "procutils.h"
#include "project.h"
#include "workspace.h"

#include <vector>
#include <wx/app.h>
#include <wx/aui/auibook.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dataview.h>
#include <wx/dcscreen.h>
#include <wx/dir.h>
#include <wx/display.h>
#include <wx/ffile.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/icon.h>
#include <wx/imaglist.h>
#include <wx/listctrl.h>
#include <wx/log.h>
#include <wx/persist.h>
#include <wx/regex.h>
#include <wx/richmsgdlg.h>
#include <wx/settings.h>
#include <wx/sstream.h>
#include <wx/stc/stc.h>
#include <wx/stdpaths.h>
#include <wx/wfstream.h>
#include <wx/window.h>
#include <wx/xrc/xmlres.h>
#include <wx/zipstrm.h>

#ifdef __WXMSW__
#include "MSWDarkMode.hpp"

#include <UxTheme.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __WXGTK20__
#include <gtk/gtk.h>
#endif

#if USE_SFTP
#include "SFTPBrowserDlg.h"
#endif

const wxEventType wxEVT_COMMAND_CL_INTERNAL_0_ARGS = ::wxNewEventType();
const wxEventType wxEVT_COMMAND_CL_INTERNAL_1_ARGS = ::wxNewEventType();

void MSWSetWindowDarkTheme(wxWindow* win)
{
#if defined(__WXMSW__) && 0
    MSWDarkMode::Get().SetDarkMode(win);
#else
    wxUnusedVar(win);
#endif
}

// --------------------------------------------------------
// Internal handler to handle queuing requests... end
// --------------------------------------------------------

#if defined(__WXGTK__)
#include <dirent.h>
#include <unistd.h>
#endif

struct ProjListComparator {
    bool operator()(const ProjectPtr p1, const ProjectPtr p2) const { return p1->GetName() > p2->GetName(); }
};

static bool IsBOMFile(const char* file_name)
{
    bool res(false);
    FILE* fp = fopen(file_name, "rb");
    if (fp) {
        struct stat buff;
        if (stat(file_name, &buff) == 0) {

            // Read the first 4 bytes (or less)
            size_t size = buff.st_size;
            if (size > 4) {
                size = 4;
            }
            char* buffer = new char[size];
            if (fread(buffer, sizeof(char), size, fp) == size) {
                BOM bom(buffer, size);
                res = (bom.Encoding() != wxFONTENCODING_SYSTEM);
            }
            delete[] buffer;
        }
        fclose(fp);
    }
    return res;
}

static bool ReadBOMFile(const char* file_name, wxString& content, BOM& bom)
{
    content.Empty();

    FILE* fp = fopen(file_name, "rb");
    if (fp) {
        struct stat buff;
        if (stat(file_name, &buff) == 0) {
            size_t size = buff.st_size;
            char* buffer = new char[size + 1];
            if (fread(buffer, sizeof(char), size, fp) == size) {
                buffer[size] = 0;

                wxFontEncoding encoding(wxFONTENCODING_SYSTEM);
                size_t bomSize(size);

                if (bomSize > 4) {
                    bomSize = 4;
                }
                bom.SetData(buffer, bomSize);
                encoding = bom.Encoding();

                if (encoding != wxFONTENCODING_SYSTEM) {
                    wxCSConv conv(encoding);
                    // Skip the BOM
                    char* ptr = buffer;
                    ptr += bom.Len();
                    content = wxString(ptr, conv);

                    if (content.IsEmpty()) {
                        content = wxString::From8BitData(ptr);
                    }
                }
            }
            delete[] buffer;
        }
        fclose(fp);
    } // From8BitData
    return content.IsEmpty() == false;
}

static bool ReadFile8BitData(const char* file_name, wxString& content)
{
    content.Empty();

    FILE* fp = fopen(file_name, "rb");
    if (fp) {
        struct stat buff;
        if (stat(file_name, &buff) == 0) {
            size_t size = buff.st_size;
            char* buffer = new char[size + 1];
            if (fread(buffer, sizeof(char), size, fp) == size) {
                buffer[size] = 0;
                content = wxString::From8BitData(buffer);
            }
            delete[] buffer;
        }
        fclose(fp);
    }
    return content.IsEmpty() == false;
}

bool SendCmdEvent(int eventId, void* clientData) { return EventNotifier::Get()->SendCommandEvent(eventId, clientData); }

bool SendCmdEvent(int eventId, void* clientData, const wxString& str)
{
    return EventNotifier::Get()->SendCommandEvent(eventId, clientData, str);
}

void PostCmdEvent(int eventId, void* clientData) { EventNotifier::Get()->PostCommandEvent(eventId, clientData); }

void SetColumnText(wxListCtrl* list, long indx, long column, const wxString& rText, int imgId)
{
    wxListItem list_item;
    list_item.SetId(indx);
    list_item.SetColumn(column);
    list_item.SetMask(wxLIST_MASK_TEXT);
    list_item.SetText(rText);
    list_item.SetImage(imgId);
    list->SetItem(list_item);
}

wxString GetColumnText(wxListCtrl* list, long index, long column)
{
    wxListItem list_item;
    list_item.SetId(index);
    list_item.SetColumn(column);
    list_item.SetMask(wxLIST_MASK_TEXT);
    list->GetItem(list_item);
    return list_item.GetText();
}

bool ReadFileWithConversion(const wxString& fileName, wxString& content, wxFontEncoding encoding, BOM* bom)
{
    wxLogNull noLog;
    content.Clear();
    wxFile file(fileName, wxFile::read);

    const wxCharBuffer name = _C(fileName);
    if (file.IsOpened()) {

        // If we got a BOM pointer, test to see whether the file is BOM file
        if (bom && IsBOMFile(name.data())) {
            return ReadBOMFile(name.data(), content, *bom);
        }

        if (encoding == wxFONTENCODING_DEFAULT) {
            encoding = EditorConfigST::Get()->GetOptions()->GetFileFontEncoding();
        }

        // first try the user defined encoding (except for UTF8: the UTF8 builtin appears to be faster)
        if (encoding != wxFONTENCODING_UTF8) {
            wxCSConv fontEncConv(encoding);
            if (fontEncConv.IsOk()) {
                file.ReadAll(&content, fontEncConv);
            }
        }

        if (content.IsEmpty()) {
            // now try the Utf8
            file.ReadAll(&content, wxConvUTF8);
            if (content.IsEmpty()) {
                // try local 8 bit data
                ReadFile8BitData(name.data(), content);
            }
        }
    }
    return !content.IsEmpty();
}

bool IsValidCppIdentifier(const wxString& id)
{
    if (id.IsEmpty()) {
        return false;
    }
    // first char can be only _A-Za-z
    wxString first(id.Mid(0, 1));
    if (first.find_first_not_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") != wxString::npos) {
        return false;
    }
    // make sure that rest of the id contains only a-zA-Z0-9_
    if (id.find_first_not_of("_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != wxString::npos) {
        return false;
    }
    return true;
}

long AppendListCtrlRow(wxListCtrl* list)
{
    long item;
    list->GetItemCount() ? item = list->GetItemCount() : item = 0;

    wxListItem info;
    // Set the item display name
    info.SetColumn(0);
    info.SetId(item);
    item = list->InsertItem(info);
    return item;
}

bool CompareFileWithString(const wxString& filePath, const wxString& str)
{
    wxString content;
    if (!ReadFileWithConversion(filePath, content)) {
        return false;
    }

    wxString diskMD5 = wxMD5::GetDigest(content);
    wxString mem_MD5 = wxMD5::GetDigest(str);
    return diskMD5 == mem_MD5;
}

bool WriteFileWithBackup(const wxString& file_name, const wxString& content, bool backup)
{
    wxUnusedVar(backup);
    wxCSConv fontEncConv(EditorConfigST::Get()->GetOptions()->GetFileFontEncoding());
    return FileUtils::WriteFileContent(file_name, content, fontEncConv);
}

bool CopyToClipboard(const wxString& text)
{
    bool ret(true);

#if wxUSE_CLIPBOARD
    if (wxTheClipboard->Open()) {
        wxTheClipboard->UsePrimarySelection(false);
        if (!wxTheClipboard->SetData(new wxTextDataObject(text))) {
            ret = false;
        }
        wxTheClipboard->Close();
    } else {
        ret = false;
    }
#else // wxUSE_CLIPBOARD
    ret = false;
#endif
    return ret;
}

wxColour MakeColourLighter(wxColour color, float level) { return DrawingUtils::LightColour(color, level); }

void FillFromSemiColonString(wxArrayString& arr, const wxString& str)
{
    arr.clear();
    arr = StringUtils::BuildArgv(str);
}

namespace
{
#ifdef __WXMSW__
/// helper method:
/// run `uname -s` command and cache the output
const wxString& __uname()
{
    static wxString uname_output;
    static bool firstTime = true;

    if (firstTime) {
        firstTime = false;
        wxFileName uname;
        if (FileUtils::FindExe("uname", uname)) {
            firstTime = false;
            clDEBUG() << "Running `uname -s`..." << endl;
            wxString cmd;
            cmd << uname.GetFullPath();
            WrapWithQuotes(cmd);
            cmd << " -s";
            uname_output = ProcUtils::SafeExecuteCommand(cmd);
            clDEBUG() << uname_output << endl;
        }
    }
    return uname_output;
}
#endif
} // namespace
bool clIsMSYSEnvironment()
{
#ifdef __WXMSW__
    wxString uname_output = __uname();
    return uname_output.Lower().Contains("MSYS_NT");
#else
    return false;
#endif
}

bool clIsCygwinEnvironment()
{
#ifdef __WXMSW__
    wxString uname_output = __uname();
    return uname_output.Lower().Contains("CYGWIN_NT");
#else
    return false;
#endif
}

void WrapInShell(wxString& cmd)
{
    wxString command;
#ifdef __WXMSW__
    wxString shell = wxGetenv("COMSPEC");
    if (shell.IsEmpty()) {
        shell = "CMD.EXE";
    }
    command << shell << " /C ";
    if (cmd.StartsWith("\"") && !cmd.EndsWith("\"")) {
        command << "\"" << cmd << "\"";
    } else {
        command << cmd;
    }
    cmd = command;
#else
    command << "/bin/sh -c '";
    // escape any single quotes
    cmd.Replace("'", "\\'");
    command << cmd << "'";
    cmd = command;
#endif
}

wxString clGetUserName()
{
    wxString squashedname, name = wxGetUserId();

    // The wx doc says that 'name' may now be e.g. "Mr. John Smith"
    // So try to make it more suitable to be an extension
    name.MakeLower();
    name.Replace(" ", "_");
    for (size_t i = 0; i < name.Len(); ++i) {
        wxChar ch = name.GetChar(i);
        if ((ch < 'a' || ch > 'z') && ch != '_') {
            // Non [a-z_] character: skip it
        } else {
            squashedname << ch;
        }
    }

    return (squashedname.IsEmpty() ? wxString("someone") : squashedname);
}

static void
DoReadProjectTemplatesFromFolder(const wxString& folder, std::list<ProjectPtr>& list, bool loadDefaults = true)
{
    // read all files under this directory
    if (wxFileName::DirExists(folder)) {
        DirTraverser dt("*.project");

        wxDir dir(folder);
        dir.Traverse(dt);

        const auto& files = dt.GetFiles();
        if (files.GetCount() > 0) {
            for (size_t i = 0; i < files.GetCount(); i++) {
                ProjectPtr proj(new Project());
                if (!proj->Load(files.Item(i))) {
                    // corrupted xml file?
                    clWARNING() << "Failed to load template project:" << files.Item(i) << "(corrupted XML?)" << endl;
                    continue;
                }
                list.push_back(proj);
                clDEBUG() << "Found template project:" << files[i] << "." << proj->GetName() << endl;
                // load template icon
                wxFileName fn(files.Item(i));
                fn.SetFullName("icon.png");
                if (fn.Exists()) {
                    wxBitmap bmp = wxBitmap(fn.GetFullPath(), wxBITMAP_TYPE_ANY);
                    if (bmp.IsOk() && bmp.GetWidth() == 16 && bmp.GetHeight() == 16) {
                        proj->SetIconPath(fn.GetFullPath());
                    }
                }
            }
        }
    }

    if (loadDefaults && list.empty()) {
        // if we ended up here, it means the installation got screwed up since
        // there should be at least 8 project templates !
        // create 3 default empty projects
        ProjectPtr exeProj(new Project());
        ProjectPtr libProj(new Project());
        ProjectPtr dllProj(new Project());
        libProj->Create("Static Library", wxEmptyString, folder, PROJECT_TYPE_STATIC_LIBRARY);
        dllProj->Create("Dynamic Library", wxEmptyString, folder, PROJECT_TYPE_DYNAMIC_LIBRARY);
        exeProj->Create("Executable", wxEmptyString, folder, PROJECT_TYPE_EXECUTABLE);
        list.push_back(libProj);
        list.push_back(dllProj);
        list.push_back(exeProj);
    }
}

void GetProjectTemplateList(std::list<ProjectPtr>& list)
{
    DoReadProjectTemplatesFromFolder(clStandardPaths::Get().GetProjectTemplatesDir(), list);
    DoReadProjectTemplatesFromFolder(clStandardPaths::Get().GetUserProjectTemplatesDir(), list, false);
    list.sort(ProjListComparator());
}

void MSWSetNativeTheme(wxWindow* win, const wxString& theme)
{
#if defined(__WXMSW__) && defined(_WIN64) && 0
    SetWindowTheme((HWND)win->GetHWND(), theme.c_str(), NULL);
#endif
}

void StringManager::AddStrings(size_t size,
                               const wxString* strings,
                               const wxString& current,
                               wxControlWithItems* control)
{
    m_size = size;
    m_unlocalisedStringArray = wxArrayString(size, strings);
    p_control = control;
    p_control->Clear();

    // Add each item to the control, localising as we go
    for (size_t n = 0; n < size; ++n) {
        p_control->Append(wxGetTranslation(strings[n]));
    }

    // Select in the control the currently used string
    SetStringSelection(current);
}

wxString StringManager::GetStringSelection() const
{
    wxString selection;
    // Find which localised string was selected
    int sel = p_control->GetSelection();
    if (sel != wxNOT_FOUND) {
        selection = m_unlocalisedStringArray.Item(sel);
    }

    return selection;
}

void StringManager::SetStringSelection(const wxString& str, size_t dfault /*= 0*/)
{
    if (str.IsEmpty() || m_size == 0) {
        return;
    }
    int sel = m_unlocalisedStringArray.Index(str);
    if (sel != wxNOT_FOUND) {
        p_control->SetSelection(sel);
    } else {
        if (dfault < m_size) {
            p_control->SetSelection(dfault);
        } else {
            p_control->SetSelection(0);
        }
    }
}

wxArrayString ReturnWithStringPrepended(const wxArrayString& oldarray, const wxString& str, const size_t maxsize)
{
    wxArrayString array(oldarray);
    if (!str.empty()) {
        // Remove any pre-existing identical entry
        // This avoids duplication, and allows us to prepend the current string
        // As a result, the array will be suitable for 'recently-used-strings' situations
        int index = array.Index(str);
        if (index != wxNOT_FOUND) {
            array.RemoveAt(index);
        }
        array.Insert(str, 0);
    }

    // Truncate the array if needed
    if (maxsize) {
        while (array.GetCount() > maxsize) {
            array.RemoveAt(array.GetCount() - 1);
        }
    }
    return array;
}

wxString wxImplode(const wxArrayString& arr, const wxString& glue)
{
    wxString str, tmp;
    for (size_t i = 0; i < arr.GetCount(); i++) {
        str << arr.Item(i) << glue;
    }

    if (str.EndsWith(glue, &tmp)) {
        str = tmp;
    }
    return str;
}

int wxStringToInt(const wxString& str, int defval, int minval, int maxval)
{
    long v;
    if (!str.ToLong(&v)) {
        return defval;
    }

    if (minval != -1 && v < minval) {
        return defval;
    }
    if (maxval != -1 && v > maxval) {
        return defval;
    }

    return v;
}

wxString wxIntToString(int val)
{
    wxString s;
    s << val;
    return s;
}

////////////////////////////////////////
// BOM
////////////////////////////////////////

BOM::BOM(const char* buffer, size_t len) { m_bom.AppendData(buffer, len); }

BOM::BOM() {}

BOM::~BOM() {}

wxFontEncoding BOM::Encoding()
{
    wxFontEncoding encoding = Encoding((const char*)m_bom.GetData());
    if (encoding != wxFONTENCODING_SYSTEM) {
        switch (encoding) {

        case wxFONTENCODING_UTF32BE:
        case wxFONTENCODING_UTF32LE:
            m_bom.SetDataLen(4);
            break;

        case wxFONTENCODING_UTF8:
            m_bom.SetDataLen(3);
            break;

        case wxFONTENCODING_UTF16BE:
        case wxFONTENCODING_UTF16LE:
        default:
            m_bom.SetDataLen(2);
            break;
        }
    }
    return encoding;
}

wxFontEncoding BOM::Encoding(const char* buff)
{
    // Support for BOM:
    //----------------------------------
    // 00 00 FE FF UTF-32, big-endian
    // FF FE 00 00 UTF-32, little-endian
    // FE FF       UTF-16, big-endian
    // FF FE       UTF-16, little-endian
    // EF BB BF    UTF-8
    //----------------------------------
    wxFontEncoding encoding = wxFONTENCODING_SYSTEM; /* -1 */

    static const char UTF32be[] = { 0x00, 0x00, (char)0xfe, (char)0xff };
    static const char UTF32le[] = { (char)0xff, (char)0xfe, 0x00, 0x00 };
    static const char UTF16be[] = { (char)0xfe, (char)0xff };
    static const char UTF16le[] = { (char)0xff, (char)0xfe };
    static const char UTF8[] = { (char)0xef, (char)0xbb, (char)0xbf };

    if (memcmp(buff, UTF32be, sizeof(UTF32be)) == 0) {
        encoding = wxFONTENCODING_UTF32BE;

    } else if (memcmp(buff, UTF32le, sizeof(UTF32le)) == 0) {
        encoding = wxFONTENCODING_UTF32LE;

    } else if (memcmp(buff, UTF16be, sizeof(UTF16be)) == 0) {
        encoding = wxFONTENCODING_UTF16BE;

    } else if (memcmp(buff, UTF16le, sizeof(UTF16le)) == 0) {
        encoding = wxFONTENCODING_UTF16LE;

    } else if (memcmp(buff, UTF8, sizeof(UTF8)) == 0) {
        encoding = wxFONTENCODING_UTF8;
    }
    return encoding;
}

void BOM::SetData(const char* buffer, size_t len)
{
    m_bom = wxMemoryBuffer();
    m_bom.SetDataLen(0);
    m_bom.AppendData(buffer, len);
}

int BOM::Len() const { return m_bom.GetDataLen(); }

void BOM::Clear()
{
    m_bom = wxMemoryBuffer();
    m_bom.SetDataLen(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////
// UTF8/16 conversions methods copied from wxScintilla
///////////////////////////////////////////////////////////////////////////////////////////////
enum { SURROGATE_LEAD_FIRST = 0xD800 };
enum { SURROGATE_TRAIL_FIRST = 0xDC00 };
enum { SURROGATE_TRAIL_LAST = 0xDFFF };

unsigned int clUTF8Length(const wchar_t* uptr, unsigned int tlen)
{
    unsigned int len = 0;
    for (unsigned int i = 0; i < tlen && uptr[i];) {
        unsigned int uch = uptr[i];
        if (uch < 0x80) {
            len++;
        } else if (uch < 0x800) {
            len += 2;
        } else if ((uch >= SURROGATE_LEAD_FIRST) && (uch <= SURROGATE_TRAIL_LAST)) {
            len += 4;
            i++;
        } else {
            len += 3;
        }
        i++;
    }
    return len;
}

// void UTF8FromUTF16(const wchar_t *uptr, unsigned int tlen, char *putf, unsigned int len)
//{
//    int k = 0;
//    for (unsigned int i = 0; i < tlen && uptr[i];) {
//        unsigned int uch = uptr[i];
//        if (uch < 0x80) {
//            putf[k++] = static_cast<char>(uch);
//        } else if (uch < 0x800) {
//            putf[k++] = static_cast<char>(0xC0 | (uch >> 6));
//            putf[k++] = static_cast<char>(0x80 | (uch & 0x3f));
//        } else if ((uch >= SURROGATE_LEAD_FIRST) &&
//                   (uch <= SURROGATE_TRAIL_LAST)) {
//            // Half a surrogate pair
//            i++;
//            unsigned int xch = 0x10000 + ((uch & 0x3ff) << 10) + (uptr[i] & 0x3ff);
//            putf[k++] = static_cast<char>(0xF0 | (xch >> 18));
//            putf[k++] = static_cast<char>(0x80 | ((xch >> 12) & 0x3f));
//            putf[k++] = static_cast<char>(0x80 | ((xch >> 6) & 0x3f));
//            putf[k++] = static_cast<char>(0x80 | (xch & 0x3f));
//        } else {
//            putf[k++] = static_cast<char>(0xE0 | (uch >> 12));
//            putf[k++] = static_cast<char>(0x80 | ((uch >> 6) & 0x3f));
//            putf[k++] = static_cast<char>(0x80 | (uch & 0x3f));
//        }
//        i++;
//    }
//    putf[len] = '\0';
//}

// unsigned int UTF8CharLength(unsigned char ch)
//{
//    if (ch < 0x80) {
//        return 1;
//    } else if (ch < 0x80 + 0x40 + 0x20) {
//        return 2;
//    } else if (ch < 0x80 + 0x40 + 0x20 + 0x10) {
//        return 3;
//    } else {
//        return 4;
//    }
//}

// unsigned int UTF16Length(const char *s, unsigned int len)
//{
//    unsigned int ulen = 0;
//    unsigned int charLen;
//    for (unsigned int i=0; i<len;) {
//        unsigned char ch = static_cast<unsigned char>(s[i]);
//        if (ch < 0x80) {
//            charLen = 1;
//        } else if (ch < 0x80 + 0x40 + 0x20) {
//            charLen = 2;
//        } else if (ch < 0x80 + 0x40 + 0x20 + 0x10) {
//            charLen = 3;
//        } else {
//            charLen = 4;
//            ulen++;
//        }
//        i += charLen;
//        ulen++;
//    }
//    return ulen;
//}
//
// unsigned int UTF16FromUTF8(const char *s, unsigned int len, wchar_t *tbuf, unsigned int tlen)
//{
//    unsigned int ui=0;
//    const unsigned char *us = reinterpret_cast<const unsigned char *>(s);
//    unsigned int i=0;
//    while ((i<len) && (ui<tlen)) {
//        unsigned char ch = us[i++];
//        if (ch < 0x80) {
//            tbuf[ui] = ch;
//        } else if (ch < 0x80 + 0x40 + 0x20) {
//            tbuf[ui] = static_cast<wchar_t>((ch & 0x1F) << 6);
//            ch = us[i++];
//            tbuf[ui] = static_cast<wchar_t>(tbuf[ui] + (ch & 0x7F));
//        } else if (ch < 0x80 + 0x40 + 0x20 + 0x10) {
//            tbuf[ui] = static_cast<wchar_t>((ch & 0xF) << 12);
//            ch = us[i++];
//            tbuf[ui] = static_cast<wchar_t>(tbuf[ui] + ((ch & 0x7F) << 6));
//            ch = us[i++];
//            tbuf[ui] = static_cast<wchar_t>(tbuf[ui] + (ch & 0x7F));
//        } else {
//            // Outside the BMP so need two surrogates
//            int val = (ch & 0x7) << 18;
//            ch = us[i++];
//            val += (ch & 0x3F) << 12;
//            ch = us[i++];
//            val += (ch & 0x3F) << 6;
//            ch = us[i++];
//            val += (ch & 0x3F);
//            tbuf[ui] = static_cast<wchar_t>(((val - 0x10000) >> 10) + SURROGATE_LEAD_FIRST);
//            ui++;
//            tbuf[ui] = static_cast<wchar_t>((val & 0x3ff) + SURROGATE_TRAIL_FIRST);
//        }
//        ui++;
//    }
//    return ui;
//}

// [CHANGED] BEGIN
// void UTF8FromUCS2(const wchar_t *uptr, unsigned int tlen, char *putf, unsigned int len)
//{
//    int k = 0;
//    for (unsigned int i = 0; i < tlen && uptr[i]; i++) {
//        unsigned int uch = uptr[i];
//        if (uch < 0x80) {
//            putf[k++] = static_cast<char>(uch);
//        } else if (uch < 0x800) {
//            putf[k++] = static_cast<char>(0xC0 | (uch >> 6));
//            putf[k++] = static_cast<char>(0x80 | (uch & 0x3f));
//        } else {
//            putf[k++] = static_cast<char>(0xE0 | (uch >> 12));
//            putf[k++] = static_cast<char>(0x80 | ((uch >> 6) & 0x3f));
//            putf[k++] = static_cast<char>(0x80 | (uch & 0x3f));
//        }
//    }
//    putf[len] = '\0';
//}

// unsigned int UCS2Length(const char *s, unsigned int len)
//{
//    unsigned int ulen = 0;
//    for (unsigned int i=0; i<len; i++) {
//        unsigned char ch = static_cast<unsigned char>(s[i]);
//        if ((ch < 0x80) || (ch > (0x80 + 0x40)))
//            ulen++;
//    }
//    return ulen;
//}

// unsigned int UCS2FromUTF8(const char *s, unsigned int len, wchar_t *tbuf, unsigned int tlen)
//{
//    unsigned int ui=0;
//    const unsigned char *us = reinterpret_cast<const unsigned char *>(s);
//    unsigned int i=0;
//    while ((i<len) && (ui<tlen)) {
//        unsigned char ch = us[i++];
//        if (ch < 0x80) {
//            tbuf[ui] = ch;
//        } else if (ch < 0x80 + 0x40 + 0x20) {
//            tbuf[ui] = static_cast<wchar_t>((ch & 0x1F) << 6);
//            ch = us[i++];
//            tbuf[ui] = static_cast<wchar_t>(tbuf[ui] + (ch & 0x7F));
//        } else {
//            tbuf[ui] = static_cast<wchar_t>((ch & 0xF) << 12);
//            ch = us[i++];
//            tbuf[ui] = static_cast<wchar_t>(tbuf[ui] + ((ch & 0x7F) << 6));
//            ch = us[i++];
//            tbuf[ui] = static_cast<wchar_t>(tbuf[ui] + (ch & 0x7F));
//        }
//        ui++;
//    }
//    return ui;
//}
// [CHANGED] END

wxString DbgPrependCharPtrCastIfNeeded(const wxString& expr, const wxString& exprType)
{
    static wxRegEx reConstArr("(const )?[ ]*(w)?char(_t)? *[\\[0-9\\]]*");

    bool arrayAsCharPtr = false;
    DebuggerInformation info;
    IDebugger* dbgr = DebuggerMgr::Get().GetActiveDebugger();
    if (dbgr) {
        DebuggerMgr::Get().GetDebuggerInformation(dbgr->GetName(), info);
        arrayAsCharPtr = info.charArrAsPtr;
    }

    wxString newExpr;
    if (arrayAsCharPtr && reConstArr.Matches(exprType)) {
        // array
        newExpr << "(char*)" << expr;

    } else {
        newExpr << expr;
    }
    return newExpr;
}

wxVariant MakeIconText(const wxString& text, const wxBitmap& bmp)
{
    wxIcon icn;
    icn.CopyFromBitmap(bmp);
    wxDataViewIconText ict(text, icn);
    wxVariant v;
    v << ict;
    return v;
}

wxArrayString SplitString(const wxString& inString, bool trim)
{
    wxArrayString lines;
    wxString curline;

    bool inContinuation = false;
    for (size_t i = 0; i < inString.length(); ++i) {
        wxChar ch = inString.GetChar(i);
        wxChar ch1 = (i + 1 < inString.length()) ? inString.GetChar(i + 1) : wxUniChar(0);
        wxChar ch2 = (i + 2 < inString.length()) ? inString.GetChar(i + 2) : wxUniChar(0);

        switch (ch) {
        case '\r':
            // do nothing
            curline << ch;
            break;
        case '\n':
            if (inContinuation) {
                curline << ch;

            } else {
                lines.Add(trim ? curline.Trim().Trim(false) : curline);
                curline.clear();
            }
            inContinuation = false;
            break;
        case '\\':
            curline << ch;
            if ((ch1 == '\n') || (ch1 == '\r' && ch2 == '\n')) {
                inContinuation = true;
            }
            break;
        default:
            curline << ch;
            inContinuation = false;
            break;
        }
    }

    // any leftovers?
    if (curline.IsEmpty() == false) {
        lines.Add(trim ? curline.Trim().Trim(false) : curline);
        curline.clear();
    }
    return lines;
}

void LaunchTerminalForDebugger(const wxString& title, wxString& tty, wxString& realPts, long& pid)
{
    pid = wxNOT_FOUND;
    tty.Clear();
    realPts.Clear();

#if defined(__WXMSW__)
    // Windows
    wxUnusedVar(title);
#else
    // Non Windows machines
    clConsoleBase::Ptr_t console = clConsoleBase::GetTerminal();
    if (console->StartForDebugger()) {
        tty = console->GetTty();
        realPts = console->GetRealPts();
        pid = console->GetPid();
    }
#endif // !__WXMSW__
}

wxStandardID PromptForYesNoCancelDialogWithCheckbox(const wxString& message,
                                                    const wxString& dlgId,
                                                    const wxString& yesLabel,
                                                    const wxString& noLabel,
                                                    const wxString& cancelLabel,
                                                    const wxString& checkboxLabel,
                                                    long style,
                                                    bool checkboxInitialValue)
{
    int res = clConfig::Get().GetAnnoyingDlgAnswer(dlgId, wxNOT_FOUND);
    if (res == wxNOT_FOUND) {

        // User did not save his answer
        wxRichMessageDialog d(EventNotifier::Get()->TopFrame(), message, "CodeLite", style);
        d.ShowCheckBox(checkboxLabel);
        if (cancelLabel.empty()) {
            d.SetYesNoLabels(yesLabel, noLabel);
        } else {
            d.SetYesNoCancelLabels(yesLabel, noLabel, cancelLabel);
        }

        res = d.ShowModal();
        if (d.IsCheckBoxChecked() && (res != wxID_CANCEL)) {
            // store the user result
            clConfig::Get().SetAnnoyingDlgAnswer(dlgId, res);
        }
    }
    return static_cast<wxStandardID>(res);
}

wxStandardID PromptForYesNoDialogWithCheckbox(const wxString& message,
                                              const wxString& dlgId,
                                              const wxString& yesLabel,
                                              const wxString& noLabel,
                                              const wxString& checkboxLabel,
                                              long style,
                                              bool checkboxInitialValue)
{
    return PromptForYesNoCancelDialogWithCheckbox(
        message, dlgId, yesLabel, noLabel, "", checkboxLabel, style, checkboxInitialValue);
}

wxString& WrapWithQuotes(wxString& str)
{
    if (!str.empty() && str.Contains(" ") && !str.StartsWith("\"") && !str.EndsWith("\"")) {
        str.Prepend("\"").Append("\"");
    }
    return str;
}

bool LoadXmlFile(wxXmlDocument* doc, const wxString& filepath)
{
    wxString content;
    if (!FileUtils::ReadFileContent(filepath, content)) {
        return false;
    }

    wxStringInputStream sis(content);
    return doc->Load(sis);
}

bool SaveXmlToFile(wxXmlDocument* doc, const wxString& filename)
{
    CHECK_PTR_RET_FALSE(doc);

    wxString content;
    wxStringOutputStream sos(&content);
    if (doc->Save(sos)) {
        return FileUtils::WriteFileContent(filename, content);
    }
    return false;
}

void wxPGPropertyBooleanUseCheckbox(wxPropertyGrid* grid)
{
    grid->SetPropertyAttributeAll(wxPG_BOOL_USE_CHECKBOX, true);

    wxColour bg_colour = clSystemSettings::GetDefaultPanelColour();
    wxColour line_colour = bg_colour.ChangeLightness(60);
    wxColour text_colour = clSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    wxColour caption_colour = bg_colour;

    bool is_dark = DrawingUtils::IsDark(bg_colour);
    if (is_dark) {
        line_colour = line_colour.ChangeLightness(130);
        caption_colour = caption_colour.ChangeLightness(120);

    } else {
        line_colour = line_colour.ChangeLightness(70);
        caption_colour = caption_colour.ChangeLightness(80);
    }
    grid->SetCaptionBackgroundColour(caption_colour);
    grid->SetCaptionTextColour(text_colour);
    grid->SetLineColour(line_colour);
    grid->SetMarginColour(caption_colour);
}

void clRecalculateSTCHScrollBar(wxStyledTextCtrl* ctrl)
{
    // recalculate and set the length of horizontal scrollbar
    int maxPixel = 0;
    int startLine = ctrl->GetFirstVisibleLine();
    int endLine = startLine + ctrl->LinesOnScreen();
    if (endLine >= (ctrl->GetLineCount() - 1)) {
        endLine--;
    }

    wxString text;
    for (int i = startLine; i <= endLine; i++) {
        int visibleLine = (int)ctrl->DocLineFromVisible(i); // get actual visible line, folding may offset lines
        wxString line_text = ctrl->GetLine(visibleLine);
        text = line_text.length() > text.length() ? line_text : text;
    }

    maxPixel = ctrl->TextWidth(0, text);
    if (maxPixel == 0) {
        maxPixel++; // make sure maxPixel is valid
    }

    int currentLength = ctrl->GetScrollWidth(); // Get current scrollbar size
    if (currentLength != maxPixel) {
        // And if it is not the same, update it
        ctrl->SetScrollWidth(maxPixel);
    }
}
wxString clGetTextFromUser(
    const wxString& title, const wxString& message, const wxString& initialValue, int charsToSelect, wxWindow* parent)
{
    clGetTextFromUserDialog dialog(
        parent == NULL ? EventNotifier::Get()->TopFrame() : parent, title, message, initialValue, charsToSelect);
    if (dialog.ShowModal() == wxID_OK) {
        return dialog.GetValue();
    }
    return "";
}

static IManager* s_pluginManager = NULL;

void clSetManager(IManager* manager) { s_pluginManager = manager; }
IManager* clGetManager() { return s_pluginManager; }

void clStripTerminalColouring(const wxString& buffer, wxString& modbuffer)
{
    StringUtils::StripTerminalColouring(buffer, modbuffer);
}

bool clIsValidProjectName(const wxString& name)
{
    return name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") == wxString::npos;
}

double clGetContentScaleFactor()
{
    static bool once = false;
    static double res = 1.0;
    if (!once) {
        once = true;
#ifdef __WXGTK__
        GdkScreen* screen = gdk_screen_get_default();
        if (screen) {
            res = gdk_screen_get_resolution(screen) / 96.;
        }
#else
        res = (wxScreenDC().GetPPI().y / 96.);
#endif
    }
    return res;
}

int clGetScaledSize(int size)
{
    if (clGetContentScaleFactor() >= 1.5) {
        return size * 2;
    } else {
        return size;
    }
}

void clKill(int processID, wxSignal signo, bool kill_whole_group, bool as_superuser)
{
#ifdef __WXMSW__
    wxUnusedVar(as_superuser);
    ::wxKill(processID, signo, NULL, kill_whole_group ? wxKILL_CHILDREN : wxKILL_NOCHILDREN);
#else
    wxString sudoAskpass = ::wxGetenv("SUDO_ASKPASS");
    const char* sudo_path = "/usr/bin/sudo";
    if (!wxFileName::Exists(sudo_path)) {
        sudo_path = "/usr/local/bin/sudo";
    }
    if (as_superuser && wxFileName::Exists(sudo_path) && wxFileName::Exists(sudoAskpass)) {
        wxString cmd;
        cmd << sudo_path << " --askpass kill -" << (int)signo << " ";
        if (kill_whole_group) {
            cmd << "-";
        }
        cmd << processID;
        int rc = system(cmd.mb_str(wxConvUTF8).data());
        wxUnusedVar(rc);
    } else {
        ::wxKill(processID, signo, NULL, kill_whole_group ? wxKILL_CHILDREN : wxKILL_NOCHILDREN);
    }
#endif
}

void clSetEditorFontEncoding(const wxString& encoding)
{
    OptionsConfigPtr options = EditorConfigST::Get()->GetOptions();
    options->SetFileFontEncoding(encoding);
    EditorConfigST::Get()->SetOptions(options);
}

int clFindMenuItemPosition(wxMenu* menu, int menuItemId)
{
    if (!menu) {
        return wxNOT_FOUND;
    }
    const wxMenuItemList& list = menu->GetMenuItems();
    wxMenuItemList::const_iterator iter = list.begin();
    for (int pos = 0; iter != list.end(); ++iter, ++pos) {
        if ((*iter)->GetId() == menuItemId) {
            return pos;
        }
    }
    return wxNOT_FOUND;
}

wxString clJoinLinesWithEOL(const wxArrayString& lines, int eol)
{
    wxString glue = "\n";
    switch (eol) {
    case wxSTC_EOL_CRLF:
        glue = "\r\n";
        break;
    case wxSTC_EOL_CR:
        glue = "\r";
        break;
    default:
        glue = "\n";
        break;
    }
    return clJoin(lines, glue);
}

wxSize clGetDisplaySize()
{
    // Calculate the display size only once. If the user changes the display size, he will need to restart CodeLite
    static wxSize displaySize;
    if (displaySize.GetHeight() == 0) {

        int displayWidth = ::wxGetDisplaySize().GetWidth();
        int displayHeight = ::wxGetDisplaySize().GetHeight();
        for (size_t i = 0; i < wxDisplay::GetCount(); ++i) {
            wxDisplay display(i);
            displayWidth = wxMax(display.GetClientArea().GetWidth(), displayWidth);
            displayHeight = wxMax(display.GetClientArea().GetHeight(), displayHeight);
        }
        displaySize = wxSize(displayWidth, displayHeight);
    }
    return displaySize;
}

void clFitColumnWidth(wxDataViewCtrl* ctrl)
{
#ifndef __WXOSX__
    for (size_t i = 0; i < ctrl->GetColumnCount(); ++i) {
        ctrl->GetColumn(i)->SetWidth(wxCOL_WIDTH_AUTOSIZE);
    }
#endif
}

wxVariant MakeBitmapIndexText(const wxString& text, int imgIndex)
{
    clDataViewTextBitmap tb(text, imgIndex);
    wxVariant vr;
    vr << tb;
    return vr;
}

wxVariant MakeCheckboxVariant(const wxString& label, bool checked, int imgIndex)
{
    clDataViewCheckbox cb(checked, imgIndex, label);
    wxVariant vr;
    vr << cb;
    return vr;
}

void clSetTLWindowBestSizeAndPosition(wxWindow* win)
{
    CHECK_PTR_RET(win);

    // confirm that this window is top level
    wxTopLevelWindow* tlw = dynamic_cast<wxTopLevelWindow*>(win);

    // find its parent
    wxWindow* parent_tlw = EventNotifier::Get()->TopFrame();

    if (!tlw || !parent_tlw) {
        return;
    }

    wxRect frameSize = parent_tlw->GetSize();
    frameSize.Deflate(100);
    tlw->SetMinSize(frameSize.GetSize());
    tlw->SetSize(frameSize.GetSize());
    tlw->CentreOnParent();

#if defined(__WXMAC__) || defined(__WXMSW__)
    tlw->Move(wxNOT_FOUND, parent_tlw->GetPosition().y);
#endif
    tlw->PostSizeEvent();
}

static void DoSetDialogSize(wxDialog* win, double factor)
{
    if (!win) {
        return;
    }

    if (factor <= 0.0) {
        factor = 1.0;
    }

    wxWindow* parent = win->GetParent();
    if (!parent) {
        parent = wxTheApp->GetTopWindow();
    }
    if (parent) {
        wxSize parentSize = parent->GetSize();

        double dlgWidth = (double)parentSize.GetWidth() * factor;
        double dlgHeight = (double)parentSize.GetHeight() * factor;
        parentSize.SetWidth(dlgWidth);
        parentSize.SetHeight(dlgHeight);
        win->SetSize(parentSize);
        win->GetSizer()->Layout();
        win->CentreOnParent();
#if defined(__WXMAC__) || defined(__WXMSW__)
        win->Move(wxNOT_FOUND, parent->GetPosition().y);
#endif
    }
}

std::pair<wxString, wxString>
clRemoteFolderSelector(const wxString& title, const wxString& accountName, wxWindow* parent)
{
#if USE_SFTP
    SFTPBrowserDlg dlg(parent, title, wxEmptyString, clSFTP::SFTP_BROWSE_FOLDERS, accountName);
    if (dlg.ShowModal() != wxID_OK) {
        return {};
    }
    return { dlg.GetAccount(), dlg.GetPath() };
#else
    return {};
#endif
}

std::pair<wxString, wxString>
clRemoteFileSelector(const wxString& title, const wxString& accountName, const wxString& filter, wxWindow* parent)
{
#if USE_SFTP
    SFTPBrowserDlg dlg(parent, title, filter, clSFTP::SFTP_BROWSE_FOLDERS | clSFTP::SFTP_BROWSE_FILES, accountName);
    if (dlg.ShowModal() != wxID_OK) {
        return {};
    }
    return { dlg.GetAccount(), dlg.GetPath() };
#else
    return {};
#endif
}

void clSetDialogBestSizeAndPosition(wxDialog* win) { DoSetDialogSize(win, 0.66); }

void clSetSmallDialogBestSizeAndPosition(wxDialog* win) { DoSetDialogSize(win, 0.5); }

void clSetDialogSizeAndPosition(wxDialog* win, double ratio) { DoSetDialogSize(win, ratio); }

bool clIsCxxWorkspaceOpened() { return clCxxWorkspaceST::Get()->IsOpen() || clFileSystemWorkspace::Get().IsOpen(); }

int clGetSize(int size, const wxWindow* win)
{
    if (!win) {
        return size;
    }
#ifdef __WXGTK__
    wxString dpiscale = "1.0";
    if (wxGetEnv("GDK_DPI_SCALE", &dpiscale)) {
        double scale = 1.0;
        if (dpiscale.ToDouble(&scale)) {
            double scaledSize = scale * size;
            return scaledSize;
        }
    }
#endif
#if wxCHECK_VERSION(3, 1, 0)
    return win->FromDIP(size);
#else
    return size;
#endif
}

bool clIsWaylandSession()
{
#ifdef __WXGTK__
    // Try to detect if this is a Wayland session; we have some Wayland-workaround code
    wxString sesstype("XDG_SESSION_TYPE"), session_type;
    wxGetEnv(sesstype, &session_type);
    return session_type.Lower().Contains("wayland");
#else
    return false;
#endif
}

bool clShowFileTypeSelectionDialog(wxWindow* parent, const wxArrayString& initial_selection, wxArrayString* selected)
{
    SelectFileTypesDialog dlg(parent, initial_selection);
    if (dlg.ShowModal() != wxID_OK) {
        return false;
    }

    auto res = dlg.GetValue();
    selected->swap(res);
    return true;
}

bool SetBestFocus(wxWindow* win)
{
    if (win->IsEnabled()) {
        if (dynamic_cast<wxBookCtrlBase*>(win)) {
            auto book = dynamic_cast<wxBookCtrlBase*>(win);
            if (book->GetPageCount()) {
                book->GetPage(book->GetSelection())->CallAfter(&wxWindow::SetFocus);
            }
            return true;
        } else if (dynamic_cast<Notebook*>(win)) {
            auto book = dynamic_cast<Notebook*>(win);
            if (book->GetCurrentPage()) {
                book->GetCurrentPage()->CallAfter(&wxWindow::SetFocus);
            }
            return true;
        } else if (dynamic_cast<wxStyledTextCtrl*>(win)) {
            win->CallAfter(&wxStyledTextCtrl::SetFocus);
            return true;
        } else if (dynamic_cast<clTreeCtrl*>(win)) {
            win->CallAfter(&clTreeCtrl::SetFocus);
            return true;
        }
    }

    // try the children
    auto children = win->GetChildren();
    for (auto c : children) {
        if (SetBestFocus(c)) {
            return true;
        }
    }
    return false;
}

Notebook* FindNotebookParentOf(wxWindow* child)
{
    if (!child) {
        return nullptr;
    }

    wxWindow* parent = child->GetParent();
    while (parent) {
        Notebook* book = dynamic_cast<Notebook*>(parent);
        if (book) {
            return book;
        }
        parent = parent->GetParent();
    }
    return nullptr;
}

bool IsChildOf(wxWindow* child, wxWindow* parent)
{
    if (child == nullptr || parent == nullptr) {
        return false;
    }

    wxWindow* curparent = child->GetParent();
    while (curparent) {
        if (curparent == parent) {
            return true;
        }
        curparent = curparent->GetParent();
    }
    return false;
}

wxColour GetRandomColour()
{
    int r = std::rand() % 256;
    int g = std::rand() % 256;
    int b = std::rand() % 256;

    wxColour c(r, g, b);
    if (clSystemSettings::GetAppearance().IsDark() && DrawingUtils::IsDark(c)) {
        return c.ChangeLightness(130);
    } else if (!clSystemSettings::GetAppearance().IsDark() && !DrawingUtils::IsDark(c)) {
        return c.ChangeLightness(70);
    } else {
        return c;
    }
}

wxString clGetVisibleSelection(wxStyledTextCtrl* ctrl)
{
    CHECK_PTR_RET_EMPTY_STRING(ctrl);

    int start_pos = ctrl->GetSelectionStart();
    int end_pos = ctrl->GetSelectionEnd();
    CHECK_COND_RET_EMPTY_STRING(end_pos > start_pos);

    // Make sure we only pick visible chars (embedded ANSI colour can break the selected word)
    wxString res;
    res.reserve(end_pos - start_pos + 1);
    for (; start_pos < end_pos; start_pos++) {
        if (ctrl->StyleGetVisible(ctrl->GetStyleAt(start_pos))) {
            res << (wxChar)ctrl->GetCharAt(start_pos);
        }
    }
    return res;
}

int GetClangFormatIntProperty(const wxString& clang_format_content, const wxString& name)
{
    auto lines = ::wxStringTokenize(clang_format_content, "\r\n", wxTOKEN_STRTOK);
    for (auto& line : lines) {
        auto prop = line.BeforeFirst(':');
        auto value = line.AfterFirst(':');

        prop.Trim(false).Trim();
        value.Trim(false).Trim();
        if (prop == name) {
            long nValue = wxNOT_FOUND;
            if (value.ToCLong(&nValue)) {
                return nValue;
            }
            break;
        }
    }
    return wxNOT_FOUND;
}
