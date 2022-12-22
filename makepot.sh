#!/bin/sh

# C/C++ files
CL_ALL_CPP=`find ./ -type f \( -iname '*.c' -or -iname '*.cpp' -or -iname '*.h' -or -iname '*.hpp' \) ! -path './ctags/*' -print`

# menu.xrc file
CL_MENU_XRC='./Runtime/rc/menu.xrc'
CL_MENU_C='/tmp/cl-menu.c'
wxrc -g -o $CL_MENU_C $CL_MENU_XRC

# accelerators.conf.default file
# CL_ACCEL_CONF='./Runtime/config/accelerators.conf.default'
# CL_ACCEL_C='/tmp/cl-accel.c'
# awk -F'|' '{printf "#line %d \"%s\"\n_(\"%s\");\n#line %d \"%s\"\n_(\"%s\");\n", NR, FILENAME, $2, NR, FILENAME, $3}' $CL_ACCEL_CONF > $CL_ACCEL_C

# Generate codelite.pot
CL_POT='./translations/codelite.pot'
xgettext -F -kwxGetTranslation -k_ -kwxTRANSLATE -kwxPLURAL:1,2 -kwxGETTEXT_IN_CONTEXT:1c,2 -kwxGETTEXT_IN_CONTEXT_PLURAL:1c,2,3 --copyright-holder='Eran Ifrah' --package-name=CodeLite -o $CL_POT $CL_ALL_CPP $CL_MENU_C
