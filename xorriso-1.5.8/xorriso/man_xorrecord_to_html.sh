#!/bin/sh

#
# man_xorrecord_to_html.sh - ts A80118 , B10309 , B50730
#
# Generates a HTML version of man page xorrecord.1
#
# To be executed in the libisoburn toplevel directory (eg. ./libisoburn-0.1.0)
#

# set -x

man_dir=$(pwd)"/xorriso"
export MANPATH="$man_dir"
manpage="xorrecord"
raw_html=$(pwd)/"xorriso/raw_man_1_xorrecord.html"
htmlpage=$(pwd)/"xorriso/man_1_xorrecord.html"

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
  -e 's/<meta name="Content-Style" content="text\/css">/<meta name="Content-Style" content="text\/css"><META NAME="description" CONTENT="man page of xorriso"><META NAME="keywords" CONTENT="man xorrecord, manual, xorrecord, CD, DVD, BD, cdrecord, compatible"><META NAME="robots" CONTENT="follow">/' \
  -e 's/<title>XORRECORD<\/title>/<title>man 1 xorrecord<\/title>/' \
  -e 's/<h1 align=center>XORRECORD<\/h1>/<h1 align=center>man 1 xorrecord<\/h1>/' \
  -e 's/<body>/<body BGCOLOR="#F5DEB3" TEXT=#000000 LINK=#0000A0 VLINK=#800000>/' \
  -e 's/<b>MMC, Session, Track, Media types: <br> MMC<\/b>/<b>MMC, Session, Track, Media types:<\/b><BR>\&nbsp;<BR><b>MMC<\/b>/' \
  -e 's/<b>Drive preparation and addressing:<\/b>/<b>Drive preparation and addressing:<\/b><BR>/' \
  -e 's/<b>Relation to program xorriso: <br> xorrecord<\/b>/<b>Relation to program xorriso:<\/b><BR>\&nbsp;<BR><b>xorrecord<\/b>/' \
  -e 's/<b>Addressing the drive: <br> --devices<\/b>/<b>Addressing the drive: <\/b><BR>\&nbsp;<BR><b>--devices<\/b>/' \
  -e 's/EXAMPLES):<br>/<A HREF="#EXAMPLES">EXAMPLES<\/A>):<br>/' \
  -e 's/<b>Inquiring drive and media:<\/b>/\&nbsp;<BR><b>Inquiring drive and media:<\/b><BR>\&nbsp;<BR>/' \
  -e 's/<b>Settings for the burn run:<\/b>/\&nbsp;<BR><b>Settings for the burn run:<\/b><BR>/' \
  -e 's/<b><br> blank=mode<\/b>/<BR>\&nbsp;<BR><b>blank=mode<\/b>/' \
  -e 's/<b>Program version and verbosity: <br> &minus;version<\/b>/\&nbsp;<BR><b>Program version and verbosity:<\/b><BR>\&nbsp;<BR><b>-version<\/b>/' \
  -e 's/<b>Options not compatible to cdrecord: <br> --no_rc<\/b>/\&nbsp;<BR><b>Options not compatible to cdrecord:<\/b><BR>\&nbsp;<BR><b>--no_rc<\/b>/' \
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
