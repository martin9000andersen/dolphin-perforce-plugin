#! /bin/sh
$EXTRACTRC *.kcfg >> rc.cpp
$XGETTEXT *.cpp -o $podir/fileviewperforceplugin.pot
