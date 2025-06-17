#!/bin/bash

if [[ $1 = '--help' ]]; then
    echo "Usage: $0 [--update-po-files]"
    exit 0
fi

# Check for the prerequisite tools
for tool in xgettext wxrc; do
    if ! command -v "$tool" &>/dev/null; then
        echo "$0: You must install '$tool' to run this script."
        exit 1
    fi
done

# Ensure that this script is running at the CodeLite source tree
if ! cd -- "$(dirname -- "${BASH_SOURCE[0]}")"; then
    echo "$0: Could not change current directory."
    exit 1
fi

# C/C++ files
mapfile -t CL_ALL_CPP < <(
    find . \
        \( \( \
        -path './sdk/*' -o \
        -path './submodules/*' -o \
        -path './Runtime/*' \) \
        -prune \) -o \
        \( -type f \
        \( -iname '*.c' -o \
        -iname '*.cpp' -o \
        -iname '*.h' -o \
        -iname '*.hpp' \) \
        -print \)
)

# menu.xrc file
CL_MENU_XRC='./Runtime/rc/menu.xrc'
CL_MENU_C=$(mktemp --suffix '.c')
wxrc -g -o "$CL_MENU_C" "$CL_MENU_XRC"

# Generate codelite.pot
CL_POT='./translations/codelite.pot'
copyright_year="2007-$(date '+%Y')"

echo "$0: Generating $CL_POT..."
xgettext \
    -L C++ \
    -F \
    -kwxGetTranslation \
    -k_ \
    -kwxTRANSLATE \
    -kwxPLURAL:1,2 \
    -kwxGETTEXT_IN_CONTEXT:1c,2 \
    -kwxGETTEXT_IN_CONTEXT_PLURAL:1c,2,3 \
    --package-name=CodeLite \
    -o- \
    "${CL_ALL_CPP[@]}" "$CL_MENU_C" |
    sed -e '1s/SOME DESCRIPTIVE TITLE./CodeLite pot file/' \
        -e "2s/YEAR THE PACKAGE'S COPYRIGHT HOLDER/Eran Ifrah $copyright_year/" \
        -e "4s/FIRST AUTHOR <EMAIL@ADDRESS>, YEAR/Eran Ifrah <eran.ifrah@gmail.com>, $copyright_year/" \
        >"$CL_POT"

# Update codelite.po for all languages
if [[ $1 = '--update-po-files' ]]; then
    find './translations' \
        -type f \
        -name 'codelite.po' \
        -exec echo "$0: Updating {}..." \; \
        -exec msgmerge -Uq --backup=none {} "$CL_POT" \;
fi

# Clean up
rm "$CL_MENU_C"

exit 0
