#!/bin/bash

if ! command -v xgettext &>/dev/null; then
    echo "$0: You must have xgettext installed to run this script."
    exit 1
elif ! command -v wxrc &>/dev/null; then
    echo "$0: You must have wxrc installed to run this script."
    exit 1
fi

# C/C++ files
mapfile -t CL_ALL_CPP < <(
    find ./ \
        -type f \
        \( -iname '*.c' -or \
        -iname '*.cpp' -or \
        -iname '*.h' -or \
        -iname '*.hpp' \) \
        ! -path './submodules/*' \
        -print
)

# menu.xrc file
CL_MENU_XRC='./Runtime/rc/menu.xrc'
CL_MENU_C=$(mktemp --suffix '.c')
wxrc -g -o "$CL_MENU_C" "$CL_MENU_XRC"

# Generate codelite.pot
CL_POT='./translations/codelite.pot'
copyright_year="2007-$(date '+%Y')"
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

# Clean up
rm "$CL_MENU_C"

echo "$0: $CL_POT has been updated."
exit 0
