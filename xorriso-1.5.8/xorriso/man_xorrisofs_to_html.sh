#!/bin/sh

#
# man_xorrisofs_to_html.sh - ts A80118 , B10309 , B50730
#
# Generates a HTML version of man page xorrisofs.1
#
# To be executed in the libisoburn toplevel directory (eg. ./libisoburn-0.1.0)
#

# set -x

man_dir=$(pwd)"/xorriso"
export MANPATH="$man_dir"
manpage="xorrisofs"
raw_html=$(pwd)/"xorriso/raw_man_1_xorrisofs.html"
htmlpage=$(pwd)/"xorriso/man_1_xorrisofs.html"

if test -r "$man_dir"/"$manpage".1
then
  dummy=dummy
else
  echo "Cannot find readable man page source $1" >&2
  exit 1
fi

if test -e "$man_dir"/man1
then
  dummy=dummy
else
  ln -s . "$man_dir"/man1
fi

if test "$1" = "-work_as_filter"
then

#  set -x

  sed \
  -e 's/<meta name="generator" content="groff -Thtml, see www.gnu.org">/<meta name="generator" content="groff -Thtml, via man -H, via xorriso\/convert_man_to_html.sh">/' \
  -e 's/<meta name="Content-Style" content="text\/css">/<meta name="Content-Style" content="text\/css"><META NAME="description" CONTENT="man page of xorriso"><META NAME="keywords" CONTENT="man xorrisofs, manual, xorrisofs, ISO 9660, mkisofs, compatible"><META NAME="robots" CONTENT="follow">/' \
  -e 's/<title>XORRISOFS<\/title>/<title>man 1 xorrisofs<\/title>/' \
  -e 's/<h1 align=center>XORRISOFS<\/h1>/<h1 align=center>man 1 xorrisofs<\/h1>/' \
  -e 's/<body>/<body BGCOLOR="#F5DEB3" TEXT=#000000 LINK=#0000A0 VLINK=#800000>/' \
  -e 's/<b>ISO 9660, Rock Ridge, Joliet, HFS+: <br> ISO 9660<\/b>/\&nbsp;<BR><b>ISO 9660, Rock Ridge, Joliet, HFS+:<\/b><BR>\&nbsp;<BR><b>ISO 9660<\/b>/' \
  -e 's/<b>Inserting files into the ISO image: <br> xorrisofs<\/b>/\&nbsp;<BR><b>Inserting files into the ISO image:<\/b><BR>\&nbsp;<BR><b>xorrisofs<\/b>/' \
  -e 's/<b>Relation to program xorriso: <br> xorrisofs<\/b>/\&nbsp;<BR><b>Relation to program xorriso:<\/b><BR>\&nbsp;<BR><b>xorrisofs<\/b>/' \
  -e 's/EXAMPLES):<br>/<A HREF="#EXAMPLES">EXAMPLES<\/A>):<br>/' \
  -e 's/<b>Image loading:<\/b>/<b>Image loading:<\/b><BR>/' \
  -e 's/<b>Settings for file insertion: <br> \&minus;path-list<\/b>/\&nbsp;<BR><b>Settings for file insertion:<\/b><BR>\&nbsp;<BR><b>-path-list<\/b>/' \
  -e 's/<b>Settings for image production: <br> &minus;o<\/b>/\&nbsp;<BR><b>Settings for image production:<\/b><BR>\&nbsp;<BR><b>-o<\/b>/' \
  -e 's/<b>Settings for standards compliance: <br> \&minus;iso-level<\/b>/\&nbsp;<BR><b>Settings for standards compliance:<\/b><BR>\&nbsp;<BR><b>-iso-level<\/b>/' \
  -e 's/<b>Settings for standards extensions:<\/b>/\&nbsp;<BR><b>Settings for standards extensions:<\/b><BR>\&nbsp;<BR>/' \
  -e 's/<b>Settings for file hiding: <br> \&minus;hide<\/b>/\&nbsp;<BR><b>Settings for file hiding:<\/b><BR>\&nbsp;<BR><b>-hide<\/b>/' \
  -e 's/<b>ISO image ID strings:<\/b>/\&nbsp;<BR><b>ISO image ID strings:<\/b><BR>/' \
  -e 's/<b>El Torito Bootable ISO images:<\/b>/\&nbsp;<BR><b>El Torito Bootable ISO images:<\/b><BR>/' \
  -e 's/<b>System Area, MBR, GPT, APM, other boot blocks:<\/b>/\&nbsp;<BR><b>System Area, MBR, GPT, APM, other boot blocks:<\/b><BR>/' \
  -e 's/<b><br> &minus;G<\/b> disk_path/<BR>\&nbsp;<BR><b>-G<\/b> disk_path/' \
  -e 's/<b>Character sets:<\/b>/\&nbsp;<BR><b>Character sets:<\/b><BR>/' \
  -e 's/<b>Jigdo Template Extraction:<\/b>/\&nbsp;<BR><b>Jigdo Template Extraction:<\/b><BR>/' \
  -e 's/<b>Miscellaneous options: <br> &minus;print-size<\/b>/\&nbsp;<BR><b>Miscellaneous options:<\/b><BR>\&nbsp;<BR><b>-print-size<\/b>/' \
\
  -e 's/<\/body>/<BR><HR><FONT SIZE=-1><CENTER>(HTML generated from '"$manpage"'.1 on '"$(date)"' by '$(basename "$0")' )<\/CENTER><\/FONT><\/body>/' \
  -e 's/&minus;/-/g' \
  <"$2" >"$htmlpage"

  set +x

  chmod u+rw,go+r,go-w "$htmlpage"
  echo "Emerged file:"
  ls -lL "$htmlpage"

else

#  export BROWSER='cp "%s" '"$raw_html"
  export BROWSER=$(pwd)/'xorriso/unite_html_b_line "%s" '"$raw_html"
  man -H "$manpage"
#  cp "$raw_html" /tmp/x.html
  "$0" -work_as_filter "$raw_html"
  rm "$raw_html"
  rm "$man_dir"/man1

fi
