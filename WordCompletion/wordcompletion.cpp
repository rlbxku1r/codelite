#include "wordcompletion.h"

#include "ColoursAndFontsManager.h"
#include "Keyboard/clKeyboardManager.h"
#include "WordCompletionDictionary.h"
#include "WordCompletionSettingsDlg.h"
#include "WordCompletionThread.h"
#include "cl_command_event.h"
#include "event_notifier.h"
#include "globals.h"
#include "lexer_configuration.h"
#include "wxCodeCompletionBoxManager.h"

#include <wx/app.h>
#include <wx/stc/stc.h>
#include <wx/xrc/xmlres.h>

// Define the plugin entry point
CL_PLUGIN_API IPlugin* CreatePlugin(IManager* manager)
{
    return new WordCompletionPlugin(manager);
}

CL_PLUGIN_API PluginInfo* GetPluginInfo()
{
    static PluginInfo info;
    info.SetAuthor(wxT("Eran Ifrah"));
    info.SetName(wxT("Word Completion"));
    info.SetDescription(_("Suggest completion based on words typed in the editors"));
    info.SetVersion(wxT("v1.0"));
    return &info;
}

CL_PLUGIN_API int GetPluginInterfaceVersion() { return PLUGIN_INTERFACE_VERSION; }

// Helper
WordCompleter::WordCompleter(WordCompletionPlugin* plugin)
    : m_plugin(plugin)
{
    EventNotifier::Get()->Bind(wxEVT_CC_WORD_COMPLETE, &WordCompleter::OnWordComplete, this);
}

WordCompleter::~WordCompleter()
{
    EventNotifier::Get()->Unbind(wxEVT_CC_WORD_COMPLETE, &WordCompleter::OnWordComplete, this);
}

void WordCompleter::OnWordComplete(clCodeCompletionEvent& event) { m_plugin->OnWordComplete(event); }

WordCompletionPlugin::WordCompletionPlugin(IManager* manager)
    : IPlugin(manager)
{
    m_longName = _("Suggest completion based on words typed in the editor");
    m_shortName = wxT("Word Completion");

    wxTheApp->Bind(wxEVT_MENU, &WordCompletionPlugin::OnSettings, this, XRCID("text_word_complete_settings"));
    m_dictionary = new WordCompletionDictionary();
    m_completer = new WordCompleter(this);
}

WordCompletionPlugin::~WordCompletionPlugin() {}

void WordCompletionPlugin::CreateToolBar(clToolBarGeneric* toolbar) { wxUnusedVar(toolbar); }

void WordCompletionPlugin::CreatePluginMenu(wxMenu* pluginsMenu)
{
    wxMenu* menu = new wxMenu;
    menu->Append(XRCID("text_word_complete_settings"), _("Settings"));
    pluginsMenu->Append(wxID_ANY, _("Word Completion"), menu);
}

void WordCompletionPlugin::UnPlug()
{
    wxDELETE(m_dictionary);
    wxDELETE(m_completer);
    wxTheApp->Unbind(wxEVT_MENU, &WordCompletionPlugin::OnSettings, this, XRCID("text_word_complete_settings"));
}

void WordCompletionPlugin::OnWordComplete(clCodeCompletionEvent& event)
{
    event.Skip(); // Always skip this

    IEditor* activeEditor = GetEditor(event.GetFileName());
    CHECK_PTR_RET(activeEditor);

    WordCompletionSettings settings;
    settings.Load();

    // Enabled?
    if(!settings.IsEnabled()) {
        return;
    }

    // Build the suggestion list
    static wxBitmap sBmp = wxNullBitmap;
    if(!sBmp.IsOk()) {
        sBmp = m_mgr->GetStdIcons()->LoadBitmap("word");
    }

    // Filter (what the user has typed so far)
    // wxStyledTextCtrl* stc = activeEditor->GetCtrl();
    // int curPos = stc->GetCurrentPos();
    // int start = stc->WordStartPosition(stc->GetCurrentPos(), true);
    // if(curPos < start) return;

    wxString filter = event.GetWord().Lower(); // stc->GetTextRange(start, curPos);

    wxStringSet_t words = m_dictionary->GetWords();
    // Parse the current buffer (if modified), to include non saved words
    if(activeEditor->IsEditorModified()) {
        // For performance (this parsing is done in the main thread)
        // only parse the visible area of the document
        wxStringSet_t unsavedBufferWords;
        wxStyledTextCtrl* stc = activeEditor->GetCtrl();
        int startPos = stc->PositionFromLine(stc->GetFirstVisibleLine());
        int endPos = stc->GetCurrentPos();

        wxString buffer = stc->GetTextRange(startPos, endPos);
        WordCompletionThread::ParseBuffer(buffer, unsavedBufferWords);

        // Merge the results
        words.insert(unsavedBufferWords.begin(), unsavedBufferWords.end());
    }

    // Get the editor keywords and add them
    LexerConf::Ptr_t lexer = ColoursAndFontsManager::Get().GetLexerForFile(activeEditor->GetFileName().GetFullName());
    if(lexer) {
        wxString keywords;
        for(size_t i = 0; i < wxSTC_KEYWORDSET_MAX; ++i) {
            keywords << lexer->GetKeyWords(i) << " ";
        }
        wxArrayString langWords = ::wxStringTokenize(keywords, "\n\t \r", wxTOKEN_STRTOK);
        words.insert(langWords.begin(), langWords.end());
    }

    wxStringSet_t filteredSet;
    if(filter.IsEmpty()) {
        filteredSet.swap(words);
    } else {
        for (const auto& word : words) {
            wxString lcWord = word.Lower();
            if(settings.GetComparisonMethod() == WordCompletionSettings::kComparisonStartsWith) {
                if(lcWord.StartsWith(filter) && filter != word) {
                    filteredSet.insert(word);
                }
            } else {
                if(lcWord.Contains(filter) && filter != word) {
                    filteredSet.insert(word);
                }
            }
        }
    }
    wxCodeCompletionBoxEntry::Vec_t entries;
    for (const auto& text : filteredSet) {
        entries.push_back(wxCodeCompletionBoxEntry::New(text, sBmp));
    }
    event.GetEntries().insert(event.GetEntries().end(), entries.begin(), entries.end());
}

void WordCompletionPlugin::OnSettings(wxCommandEvent& event)
{
    WordCompletionSettingsDlg dlg(EventNotifier::Get()->TopFrame());
    dlg.ShowModal();
}

IEditor* WordCompletionPlugin::GetEditor(const wxString& filepath) const
{
    auto editor = clGetManager()->FindEditor(filepath);
    if(editor && editor == clGetManager()->GetActiveEditor()) {
        return editor;
    }
    return nullptr;
}
