#include "properties_list_view.h"

#include "custom_pg_properties.h"
#include "event_notifier.h"
#include "globals.h"
#include "int_property.h"
#include "wxc_project_metadata.h"
#include "wxc_widget.h"
#include "wxgui_helpers.h"
#include "wxguicraft_main_view.h"

#include <unordered_set>
#include <wx/msgdlg.h>
#include <wx/propgrid/advprops.h>
#include <wx/regex.h>

/////////////////////////////////////////////////////////////////////////

namespace
{
wxcPGChoiceAndButtonEditor* gChoiceButtonEditor = NULL;
// a list of properties that if changed, there is no need to refresh the view
std::unordered_set<wxString> DO_NOT_NOTIFY_SET = { PROP_SUBCLASS_INCLUDE, PROP_SUBCLASS_NAME, PROP_SUBCLASS_STYLE };
} // namespace

PropertiesListView::PropertiesListView(wxWindow* win)
    : wxPanel(win, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE)
    , m_wxcWidget(NULL)
{
    wxBoxSizer* sz = new wxBoxSizer(wxVERTICAL);
    SetSizer(sz);

    m_pgMgr =
        new wxPropertyGridManager(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED | wxPG_DESCRIPTION | wxBORDER_NONE);

    m_pg = m_pgMgr->GetGrid();
    m_pgMgr->Bind(wxEVT_PG_CHANGED, &PropertiesListView::OnCellChanged, this);
    m_pgMgr->Bind(wxEVT_PG_CHANGING, &PropertiesListView::OnCellChanging, this);
    m_pg->Bind(wxEVT_UPDATE_UI, &PropertiesListView::OnPropertyGridUpdateUI, this);
    sz->Add(m_pgMgr, 1, wxEXPAND);
    GetSizer()->Layout();
}

PropertiesListView::~PropertiesListView()
{
    m_pgMgr->Unbind(wxEVT_PG_CHANGED, &PropertiesListView::OnCellChanged, this);
    m_pgMgr->Unbind(wxEVT_PG_CHANGING, &PropertiesListView::OnCellChanging, this);
    m_pg->Unbind(wxEVT_UPDATE_UI, &PropertiesListView::OnPropertyGridUpdateUI, this);
}

void PropertiesListView::ConstructProjectSettings()
{
    DoClear();
    m_type = Type_Project_Metadata;

    wxString filesStr = wxCrafter::Join(wxcProjectMetadata::Get().GetIncludeFiles(), ";");
    m_pg->Append(new wxPropertyCategory(_("wxCrafter Project Settings")));
    AddDirPicker(_("Output Directory"), wxcProjectMetadata::Get().GetGeneratedFilesDir(),
                 _("The generated files' output directory"));
    AddDirPicker(wxGetTranslation(PROP_OUTPUT_FILE_NAME), wxcProjectMetadata::Get().GetOutputFileName(),
                 _("The generated files' name.\nwxCrafter will generate a $(FILE).cpp and $(FILE).hpp"));
    AddLongTextProp(_("Additional Include Files"), filesStr,
                    _("List of additional include files that should be included in the "
                      "generated files\ne.g. my_header.hpp ; string.h"));
    AddTextProp(_("Bitmap Files"), wxcProjectMetadata::Get().GetBitmapsFile(),
                _("Set here the file name for the bitmaps\nwxCrafter converts all the bitmaps in the project into C++ "
                  "code and will place them in this file"));

    m_pg->Append(new wxPropertyCategory(_("wxWindow ID Generation")));
    AddBoolProp(
        _("Generate Window ID"), wxcProjectMetadata::Get().IsUseEnum(),
        _("Auto generate enum in the base class for the wxWindow IDs which are not a stock window ID nor wrapped with "
          "the XRCID macro\nWhen left unchecked it is up to the user to provide a header file with the wxWindow IDs"));
    AddIntegerProp(_("First Window ID"), wxcProjectMetadata::Get().GetFirstWindowId(),
                   _("When 'Generate Window ID' is checked, use this as the first enumerator value"));

    m_pg->Append(new wxPropertyCategory(_("Code Generation")));
    AddBoolProp(_("Generate Translatable Strings"), wxcProjectMetadata::Get().IsUseUnderscoreMacro(),
                _("When enabled, all generated strings are wrapped with the \"_\" macro, otherwise allow users to "
                  "directly enter native text string encapsulated by wxT() macro"));
    AddBoolProp(_("Add wxWidgets Handlers if missing"), wxcProjectMetadata::Get().IsAddHandlers(),
                _("When enabled, wxCrafter will add missing handlers (e.g. wxBitmapXmlHandler)"));
}

void PropertiesListView::Construct(wxcWidget* wb)
{
    DoClear();
    m_type = Type_Control;
    m_wxcWidget = wb;
    if(!m_wxcWidget) {
        return;
    }

    // Populate the table
    for (const auto& [_, property] : wb->GetProperties()) {
        if(!property) {
            continue;
        }

        wxPGProperty* pgProp = NULL;
        if(property->GetType() == PT_CATEGORY) {
            pgProp = m_pg->Append(new wxPropertyCategory(property->GetValue()));

        } else if(property->GetType() == PT_BOOL) {
            pgProp = AddBoolProp(property->GetLabelForUI(), property->GetValue() == "1", property->GetTooltip());

        } else if(property->GetType() == PT_INT) {
            IntProperty* intProp = dynamic_cast<IntProperty*>(property.get());
            if(intProp) {
                if(intProp->IsUninitialized()) {
                    pgProp = AddIntegerProp(property->GetLabelForUI(), property->GetTooltip());
                } else {
                    pgProp = AddIntegerProp(property->GetLabelForUI(), wxCrafter::ToNumber(property->GetValue(), -1),
                                            property->GetTooltip());
                }
            }

        } else if(property->GetType() == PT_COMBOX) {
            pgProp = AddComboxProp(property->GetLabelForUI(), property->GetOptions(), property->GetValue(),
                                   property->GetTooltip());

        } else if(property->GetType() == PT_STRING) {
            pgProp = AddTextProp(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_CHOICE) {
            wxArrayString ops = property->GetOptions();
            pgProp =
                AddChoiceProp(property->GetLabelForUI(), ops, ops.Index(property->GetValue()), property->GetTooltip());

        } else if(property->GetType() == PT_MUTLI_STRING_PICKER) {
            pgProp = AddLongTextProp(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_COLOR_PICKER) {
            pgProp = AddColorProp(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_FONT_PICKER) {
            pgProp = AddFontProp(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_FILE_PICKER) {
            pgProp = AddFilePicker(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_BITMAP) {
            pgProp = AddBitmapPicker(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_BITMAPTEXT_TABLE) {
            pgProp = AddBmpTextPicker(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_VIRTUAL_FOLDER_PICKER) {
            pgProp = AddVDPicker(property->GetLabelForUI(), property->GetValue(), property->GetTooltip());

        } else if(property->GetType() == PT_COL_HEADER_FLAGS) {
            pgProp = AddFlags(property->GetLabelForUI(), property->GetOptions(), property->GetOptionsValues(),
                              property->GetValueLong(), property->GetTooltip());
        }

        if(pgProp) {
            pgProp->SetClientData(property.get());
        }
    }
    ::wxPGPropertyBooleanUseCheckbox(m_pg);
}

wxPGProperty* PropertiesListView::AddBoolProp(const wxString& label, bool checked, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxBoolProperty(label, wxPG_LABEL, checked));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddChoiceProp(const wxString& label, const wxArrayString& value, int sel,
                                                const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxEnumProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    prop->SetChoiceSelection(sel);
    return prop;
}

wxPGProperty* PropertiesListView::AddFilePicker(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop =
        m_pg->Append(new wxPG_FilePickerProperty(label, wxcProjectMetadata::Get().GetProjectPath(), wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddTextProp(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxStringProperty(label, label, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddLongTextProp(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxPG_MultiStringProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddColorProp(const wxString& label, const wxString& value, const wxString& tip)
{
    wxColourPropertyValue cpv;
    int colorIdx = wxCrafter::GetColourSysIndex(value);
    if(colorIdx == -1 && value != "<Default>") {

        wxColour c = wxCrafter::NameToColour(value);
        cpv = wxColourPropertyValue(wxPG_COLOUR_CUSTOM, c);

    } else if(colorIdx != -1) {
        cpv = wxColourPropertyValue(colorIdx);
    }

    wxPGProperty* prop = m_pg->Append(new wxPG_Colour(label, wxPG_LABEL, cpv));
    if(!gChoiceButtonEditor) {
        gChoiceButtonEditor = new wxcPGChoiceAndButtonEditor();
    }

    m_pg->SetPropertyEditor(prop, gChoiceButtonEditor);
    prop->SetHelpString(tip);

    if(value == "<Default>") {
        prop->SetValueToUnspecified();
    }
    return prop;
}

wxPGProperty* PropertiesListView::AddComboxProp(const wxString& label, const wxArrayString& options,
                                                const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxEditEnumProperty(label, wxPG_LABEL, options));

    prop->SetHelpString(tip);
    prop->SetValueFromString(value);
    return prop;
}

wxPGProperty* PropertiesListView::AddFontProp(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxPG_FontProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddBitmapPicker(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxPG_BitmapProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

void PropertiesListView::DoClear()
{
    m_type = Type_Control;
    m_pg->Clear();
    m_pgMgr->SetDescription("", "");
    m_properties.clear();
    m_wxcWidget = NULL;
}

void PropertiesListView::OnCellChanged(wxPropertyGridEvent& e)
{
    wxPGProperty* pgp = e.GetProperty();
    if(m_type == Type_Project_Metadata) {
        wxPGProperty* p = NULL;

        p = m_pg->GetProperty(_("Output Directory"));
        if(p) {
            wxcProjectMetadata::Get().SetGeneratedFilesDir(p->GetValueAsString());
        }

        p = m_pg->GetProperty(_("Additional Include Files"));
        if(p) {
            wxcProjectMetadata::Get().SetIncludeFiles(wxCrafter::Split(p->GetValueAsString(), ";"));
        }

        p = m_pg->GetProperty(_("Bitmap Files"));
        if(p) {
            wxcProjectMetadata::Get().SetBitmapsFile(p->GetValueAsString());
        }

        p = m_pg->GetProperty(wxGetTranslation(PROP_OUTPUT_FILE_NAME));
        if(p) {
            wxcProjectMetadata::Get().SetOutputFileName(p->GetValueAsString());
        }

        p = m_pg->GetProperty(_("Generate Window ID"));
        if(p) {
            wxcProjectMetadata::Get().SetUseEnum(p->GetValue().GetBool());
        }

        p = m_pg->GetProperty(_("First Window ID"));
        if(p) {
            wxcProjectMetadata::Get().SetFirstWindowId(p->GetValue().GetInteger());
        }

        p = m_pg->GetProperty(_("Generate Translatable Strings"));
        if(p) {
            wxcProjectMetadata::Get().SetUseUnderscoreMacro(p->GetValue().GetBool());
        }

        p = m_pg->GetProperty(_("Add wxWidgets Handlers if missing"));
        if(p) {
            wxcProjectMetadata::Get().SetAddHandlers(p->GetValue().GetBool());
        }

        wxCommandEvent evt(wxEVT_PROJECT_METADATA_MODIFIED);
        EventNotifier::Get()->AddPendingEvent(evt);

    } else {

        PropertyBase* pb = (PropertyBase*)pgp->GetClientData();
        if(pb) {
            // Check if the label modified is the "Name"
            // if it is the "Name" - make sure there is no other wxcWidget with this name
            if(pb->GetLabelForUI() == _("Name")) {
                if(m_wxcWidget->GetTopLevel() && !m_wxcWidget->GetTopLevel()->IsNameUnique(pgp->GetValueAsString())) {
                    pgp->SetValueFromString(pb->GetValue(), 0);
                    ::wxMessageBox(_("A control with this name already exists!"), "wxCrafter",
                                   wxICON_WARNING | wxOK | wxCENTER);
                    return;
                }
            }

            wxBoolProperty* boolProp = dynamic_cast<wxBoolProperty*>(pgp);
            if(boolProp) {
                pb->SetValue(boolProp->GetValue().GetBool() ? "1" : "0");
            } else {
                pb->SetValue(pgp->GetValueAsString());
            }

            // we dont always want to notify about changes
            if(DO_NOT_NOTIFY_SET.count(pb->GetLabel()) == 0) {
                // Notify about modifications
                wxCommandEvent evt(wxEVT_PROPERTIES_MODIFIED);
                if(pb->GetLabel() == PROP_NAME) {
                    evt.SetString(pgp->GetValueAsString());
                    evt.SetClientData(m_wxcWidget);
                }
                EventNotifier::Get()->AddPendingEvent(evt);
            }
        }
    }
}

wxPGProperty* PropertiesListView::AddVDPicker(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxPG_VDPickerProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddIntegerProp(const wxString& label, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxIntProperty(label, wxPG_LABEL));
    prop->SetValueToUnspecified();
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddIntegerProp(const wxString& label, int value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxIntProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddDirPicker(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop =
        m_pg->Append(new wxPG_DirPickerProperty(label, wxcProjectMetadata::Get().GetProjectPath(), wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}

wxPGProperty* PropertiesListView::AddFlags(const wxString& label, const wxArrayString& strings,
                                           const wxArrayInt& values, long value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxFlagsProperty(label, wxPG_LABEL, strings, values, value));
    prop->SetHelpString(tip);
    m_pg->Expand(prop);
    return prop;
}

void PropertiesListView::OnCellChanging(wxPropertyGridEvent& e)
{
    wxString propName = e.GetPropertyName();
    wxString sizeLabel = PROP_SIZE;
    sizeLabel.EndsWith(":", &sizeLabel);

    wxString minSizeLabel = PROP_MINSIZE;
    minSizeLabel.EndsWith(":", &minSizeLabel);

    // Validate Size / Minimum Size properties
    if(propName == sizeLabel || propName == minSizeLabel) {

        // size regex
        static wxRegEx reSize("[-0-9]+,[ \t]*[-0-9]+");
        wxString propValue = e.GetPropertyValue().GetString();
        propValue.Trim().Trim(false);

        // Validate against the regex (only if not empty)
        if(!propValue.IsEmpty() && !reSize.Matches(propValue)) {
            e.Veto();

        } else {
            e.Skip();
        }

    } else {
        e.Skip();
    }
}

void PropertiesListView::OnPropertyGridUpdateUI(wxUpdateUIEvent& event)
{
    // UpdateUI for whether to show the PROP_DROPDOWN_MENU property i.e. when the tool's a dropdown
    // GetPropertyByLabel() didn't work here. Nor did e.g. PROP_KIND. It has to be exactly this!
    wxPGProperty* kind = m_pgMgr->GetPropertyByLabel(_("Kind"));
    wxPGProperty* ddm = m_pgMgr->GetPropertyByLabel(_("Construct the Dropdown Menu"));

    if(kind && ddm) {
        // Doing ddm->Hide(kind->GetValueAsString() != ITEM_DROPDOWN) worked, but it was impossible to change the
        // properties value as it was constantly being refreshed
        // So only Show/Hide it when it's actually needed
        if(!ddm->HasFlag(wxPG_PROP_HIDDEN) && (kind->GetValueAsString() != ITEM_DROPDOWN)) {
            ddm->Hide(true);
        } else if(ddm->HasFlag(wxPG_PROP_HIDDEN) && (kind->GetValueAsString() == ITEM_DROPDOWN)) {
            ddm->Hide(true);
        }
    }
}

wxPGProperty* PropertiesListView::AddBmpTextPicker(const wxString& label, const wxString& value, const wxString& tip)
{
    wxPGProperty* prop = m_pg->Append(new wxPG_BmpTextProperty(label, wxPG_LABEL, value));
    prop->SetHelpString(tip);
    return prop;
}
