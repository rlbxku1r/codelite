#!/bin/sh

# C/C++ files
CL_CPP_SRC=`find ./ -type f \( -iname '*.c' -or -iname "*.cpp" -or -iname "*.h" -or -iname "*.hpp" \) -print`

# menu.xrc file
CL_XRC_SRC='/tmp/codelite-menu.c'
wxrc -g ./Runtime/rc/menu.xrc -o $CL_XRC_SRC

# Generate codelite.pot
xgettext -s --keyword=_ --keyword=wxTRANSLATE --keyword=wxPLURAL:1,2 -o ./translations/codelite.pot $CL_CPP_SRC $CL_XRC_SRC
