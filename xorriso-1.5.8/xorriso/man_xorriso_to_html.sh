#!/bin/sh

#
# man_xorriso_to_html.sh - ts A80118 , B10309 , B50730
#
# Generates a HTML version of man page xorriso.1
#
# To be executed in the libisoburn toplevel directory (eg. ./libisoburn-0.1.0)
#

# set -x

man_dir=$(pwd)"/xorriso"
export MANPATH="$man_dir"
manpage="xorriso"
raw_html=$(pwd)/"xorriso/raw_man_1_xorriso.html"
htmlpage=$(pwd)/"xorriso/man_1_xorriso.html"

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
  -e 's/<meta name="Content-Style" content="text\/css">/<meta name="Content-Style" content="text\/css"><META NAME="description" CONTENT="man page of xorriso"><META NAME="keywords" CONTENT="man xorriso, manual, xorriso, CD, CD-RW, CD-R, burning, cdrecord, compatible"><META NAME="robots" CONTENT="follow">/' \
  -e 's/<title>XORRISO<\/title>/<title>man 1 xorriso<\/title>/' \
  -e 's/<h1 align=center>XORRISO<\/h1>/<h1 align=center>man 1 xorriso<\/h1>/' \
  -e 's/<body>/<body BGCOLOR="#F5DEB3" TEXT=#000000 LINK=#0000A0 VLINK=#800000>/' \
  -e 's/<b>Overview of features:<\/b>/\&nbsp;<BR><A NAME="Overview"><\/A><b>Overview of features:<\/b>/' \
  -e 's/<b>General information paragraphs:<\/b>/\&nbsp;<BR><A NAME="General"><\/A><b>General information paragraphs:<\/b>/' \
  -e 's/Session model <br>/<A HREF="#Model">Session model<\/A> <br>/' \
  -e 's/^Media types and states/<A HREF="#Media">Media types and states<\/A>/' \
  -e 's/^Creating, Growing, Modifying, Blind Growing/<A HREF="#Methods">Creating, Growing, Modifying, Blind Growing<\/A>/' \
  -e 's/^Libburn drives/<A HREF="#Drives">Libburn drives<\/A>/' \
  -e 's/^Rock Ridge, POSIX, X\/Open, El Torito, ACL, xattr/<A HREF="#Extras">Rock Ridge, POSIX, X\/Open, El Torito, ACL, xattr<\/A>/' \
  -e 's/^Command processing/<A HREF="#Processing">Command processing<\/A>/' \
  -e 's/^Dialog, Readline, Result pager/<A HREF="#Dialog">Dialog, Readline, Result pager<\/A>/' \
  -e 's/have a look at section EXAMPLES/have a look at section <A HREF="#EXAMPLES">EXAMPLES<\/A>/' \
  -e 's/<b>Session model:<\/b>/\&nbsp;<BR><A NAME="Model"><\/A><b>Session model:<\/b><BR>/' \
  -e 's/<b>Media types and states:<\/b>/\&nbsp;<BR><A NAME="Media"><\/A><b>Media types and states:<\/b><BR>/' \
  -e 's/<b>Creating, Growing, Modifying, Blind Growing:<\/b>/\&nbsp;<BR><A NAME="Methods"><\/A><b>Creating, Growing, Modifying, Blind Growing:<\/b><BR>/' \
  -e 's/<b>Libburn drives:<\/b>/\&nbsp;<BR><b>Libburn drives:<\/b><A NAME="Drives"><\/A><BR>/' \
  -e 's/^-dev /\&nbsp;\&nbsp;-dev /' \
  -e 's/^-devices /\&nbsp;\&nbsp;-devices /' \
  -e 's/<b>Rock Ridge, POSIX, X\/Open, El Torito, ACL, xattr: <br> Rock Ridge<\/b>/\&nbsp;<BR><A NAME="Extras"><\/A><b>Rock Ridge, POSIX, X\/Open, El Torito, ACL, xattr:<\/b><BR>\&nbsp;<BR><b>Rock Ridge<\/b>/' \
  -e 's/<b>Command processing:<\/b>/\&nbsp;<BR><A NAME="Processing"><\/A><b>Command processing:<\/b><BR>/' \
  -e 's/<b>Dialog, Readline, Result pager:<\/b>/<A NAME="Dialog"><\/A><b>Dialog, Readline, Result pager:<\/b><BR>/' \
  -e 's/<b><br> Execution order of program arguments:<\/b>/<br>\&nbsp;<BR><A NAME="ArgSort"><\/A><b>Execution order of program arguments:<\/b><BR>/' \
  -e 's/<b>Acquiring source and target drive:<\/b>/\&nbsp;<BR><A NAME="AqDrive"><\/A><b>Acquiring source and target drive:<\/b><BR>/' \
  -e 's/<b>Influencing the behavior of image/\&nbsp;<BR><A NAME="Loading"><\/A><b>Influencing the behavior of image/' \
  -e 's/<b>Inserting files into ISO image:<\/b>/\&nbsp;<BR><A NAME="Insert"><\/A><b>Inserting files into ISO image:<\/b><BR>/' \
  -e 's/<b>Settings for file insertion:<\/b>/\&nbsp;<BR><A NAME="SetInsert"><\/A><b>Settings for file insertion:<\/b><BR>\&nbsp;<BR>/' \
  -e 's/<b>Tree traversal command -find: <br> \&minus;find<\/b>/\&nbsp;<BR><A NAME="CmdFind"><\/A><b>Tree traversal command -find:<\/b><BR>\&nbsp;<BR><b>-find<\/b>/' \
  -e 's/<b>File manipulations:<\/b>/\&nbsp;<BR><A NAME="Manip"><\/A><b>File manipulations:<\/b><BR>/' \
  -e 's/^<p><b>&minus;iso_rr_pattern/<p>\&nbsp;<BR><b>\&minus;iso_rr_pattern/' \
  -e 's/EXAMPLES):<br>/<A HREF="#EXAMPLES">EXAMPLES<\/A>):<br>/' \
  -e 's/<b>Filters for data file content:<\/b>/\&nbsp;<BR><A NAME="Filter"><\/A><b>Filters for data file content:<\/b><BR>/' \
  -e 's/<b>Writing the result, drive control:<\/b>/\&nbsp;<BR><A NAME="Writing"><\/A><b>Writing the result, drive control:<\/b><BR>/' \
  -e 's/^-find \/ /\&nbsp;\&nbsp;-find \/ /' \
  -e 's/^$<\/b> ln -s/\&nbsp;\&nbsp;$<\/b> ln -s/' \
  -e 's/<b>Settings for result writing:<\/b>/\&nbsp;<BR><A NAME="SetWrite"><\/A><b>Settings for result writing:<\/b><BR>/' \
  -e 's/^706k = 706kB/\&nbsp;\&nbsp;706k = 706kB/' \
  -e 's/^5540k = 5540kB/\&nbsp;\&nbsp;5540k = 5540kB/' \
  -e 's/<b>Bootable ISO images:<\/b>/\&nbsp;<BR><A NAME="Bootable"><\/A><b>Bootable ISO images:<\/b><BR>/' \
  -e 's/<b>Jigdo Template Extraction:<\/b>/\&nbsp;<BR><A NAME="Jigdo"><\/A><b>Jigdo Template Extraction:<\/b><BR>/' \
  -e 's/<b>Character sets:<\/b>/\&nbsp;<BR><A NAME="Charset"><\/A><b>Character sets:<\/b><BR>/' \
  -e 's/<b>Exception processing:<\/b>/\&nbsp;<BR><A NAME="Exception"><\/A><b>Exception processing:<\/b><BR>/' \
  -e 's/<b>Dialog mode control: <br> &minus;dialog<\/b>/\&nbsp;<BR><A NAME="DialogCtl"><\/A><b>Dialog mode control:<\/b><BR>\&nbsp;<BR><b>-dialog<\/b>/' \
  -e 's/<b>Drive and media related inquiry actions: <br> &minus;devices<\/b>/\&nbsp;<BR><A NAME="Inquiry"><\/A><b>Drive and media related inquiry actions:<\/b><BR>\&nbsp;<BR><b>-devices<\/b>/' \
  -e 's/<b>Navigation in ISO image and disk filesystem: <br> &minus;cd/\&nbsp;<BR><A NAME="Navigate"><\/A><b>Navigation in ISO image and disk filesystem:<\/b><BR>\&nbsp;<BR><b>-cd<\/b>/' \
  -e 's/<b>Evaluation of readability and recovery:<\/b>/\&nbsp;<BR><A NAME="Verify"><\/A><b>Evaluation of readability and recovery:<\/b><BR>/' \
  -e 's/<b>osirrox ISO-to-disk restore commands:<\/b>/\&nbsp;<BR><A NAME="Restore"><\/A><b>osirrox ISO-to-disk restore commands:<\/b><BR>/' \
  -e 's/<b>Command compatibility emulations:<\/b>/\&nbsp;<BR><A NAME="Emulation"><\/A><b>Command compatibility emulations:<\/b><BR>/' \
  -e 's/^<p><b>&minus;as</<p>\&nbsp;<BR><b>\&minus;as</' \
  -e 's/<b>Scripting, dialog and program control features:<\/b>/\&nbsp;<BR><A NAME="Scripting"><\/A><b>Scripting, dialog and program control features:<\/b><BR>\&nbsp;<BR>/' \
  -e 's/<b>Support for frontend programs via stdin and stdout: <br> &minus;pkt_output<\/b>/\&nbsp;<BR><A NAME="Frontend"><\/A><b>Support for frontend programs via stdin and stdout:<\/b><BR>\&nbsp;<BR><b>-pkt_output<\/b>/' \
  -e 's/disk_path_stdin disk_path_stdout <br>/disk_path_stdin disk_path_stdout/' \
  -e 's/xorriso -outdev \/dev\/sr2 \\ -blank fast \\ -pathspecs on/xorriso -outdev \/dev\/sr2 -blank fast -pathspecs on/' \
  -e 's/\\ -add \\ \/sounds=\/home\/me\/sounds \\ \/pictures \\ -- \\ -rm_r \\/ -add \/sounds=\/home\/me\/sounds \/pictures -- -rm_r /' \
  -e 's/\/sounds\/indecent \\ \&rsquo;\/pictures\/\*private\*\&rsquo; \\/\/sounds\/indecent \&rsquo;\/pictures\/*private*\&rsquo; /' \
  -e 's/\/pictures\/confidential \\ -- \\ -add \\/\/pictures\/confidential -- -add/' \
  -e 's/xorriso -dev \/dev\/sr2 \\ -rm_r \/sounds -- \\ -mv \\/xorriso -dev \/dev\/sr2 -rm_r \/sounds -- -mv /' \
  -e 's/\/pictures\/confidential \\ \/pictures\/restricted \\ -- \\ -chmod/\/pictures\/confidential \/pictures\/restricted -- -chmod/' \
  -e 's/go-rwx \/pictures\/restricted -- \\ -pathsspecs on \\ -add \\/go-rwx \/pictures\/restricted -- -pathsspecs on -add /' \
  -e 's/\/sounds=\/home\/me\/prepared_for_dvd\/sounds_dummy /\/sounds=\/home\/me\/prepared_for_dvd\/sounds_dummy/' \
  -e 's/\/movies=\/home\/me\/prepared_for_dvd\/movies \\ -- \\ -commit/\/movies=\/home\/me\/prepared_for_dvd\/movies -- -commit/' \
  -e 's/xorriso -indev \/dev\/sr2 \\ -rm_r \/sounds -- \\/xorriso -indev \/dev\/sr2 -rm_r \/sounds -- /' \
  -e 's/-outdev \/dev\/sr0 -blank fast \\ -commit -eject all/-outdev \/dev\/sr0 -blank fast -commit -eject all/' \
  -e 's/See section FILES/See section <A HREF="#FILES">FILES<\/A>/' \
  -e 's/See section EXAMPLES/See section <A HREF="#EXAMPLES">EXAMPLES<\/A>/' \
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
