#!/bin/sh

# make_xorriso_standalone.sh
# Copyright 2008 - 2026 Thomas Schmitt, scdbackup@gmx.net, GPLv2+
#
# Not intended for general use in production installations !
# 
# This is a development tool which expects a special setup of directories.
# It creates a new directory tree
#   xorriso-standalone
# for building GNU xorriso.
# The ./bootstrap script gets applied and a source tarball is made.
#
# This development tool is to be executed in a common parent of the directories 
#   nglibisofs-develop jte-develop libburn-develop libisoburn-develop
# where tarballs have been unpacked or repository copies have been cloned of
#   libisofs           jte         libburn         libisoburn
# according to instructions on http://libburnia-project.org.
# automake and libtool have to be installed on the system.
#
# Further it is necessary to build a little binary for HTML man page production
#   (cd libisoburn-develop/xorriso && \
#    cc -g -Wall -o unite_html_b_line unite_html_b_line.c )
#
# Then run
#
#   ./libisoburn-develop/xorriso/make_xorriso_standalone.sh
#
# From the emerging xorriso-standalone tarball can be build a binary
#   xorriso/xorriso
# which at runtime does not depend on installed libburnia libraries.
# Execute in xorriso-standalone :
#
#   ./configure && make
#

# By the variable setting  create_gnu_xorriso="yes"
# the result will become a GNU xorriso tarball under GPLv3+.
# Without this setting, the result is technically equivalent but
# stays under GPLv2+. In that case the files xorriso/*_gnu_xorriso
# are merely informative.
# Note that it is not permissible to revert the transition from
# GPLv2+ to GPLv3+. (Rather derive a new GPLv2+ from libburnia.)

create_gnu_xorriso="yes"


current_dir=$(pwd)
lone_dir="$current_dir"/"xorriso-standalone"

xorriso_rev=1.5.8
# For unstable uploads and patch level 0 of stable releases:
xorriso_pl=""
# For higher patch levels of stable releases:
## xorriso_pl=".pl01"

with_bootstrap_tarball=1

create_dir() {
  if mkdir "$1"
  then
    dummy=dummy
  else
    echo "Failed to create : $1" >&2
    exit 1
  fi
}

goto_dir() {
  if cd "$1"
  then
    dummy=dummy
  else
    echo "Failed to cd $1" >&2
    exit 1
  fi
}

copy_files() {
  if cp "$@"
  then
    dummy=dummy
  else
    echo "Failed to : cp " "$@" >&2
    exit 1
  fi
}

copy_tree() {
  if cp -a "$@"
  then
    dummy=dummy
  else
    echo "Failed to : cp -a " "$@" >&2
    exit 1
  fi
}

if test -e "$lone_dir"
then
  echo "Already existing : $lone_dir" >&2
  exit 1
fi


# Top level directory 

goto_dir "$current_dir"/libisoburn-develop

create_dir "$lone_dir"

copy_files \
 AUTHORS \
 CONTRIBUTORS \
 COPYRIGHT \
 COPYING \
 ChangeLog \
 INSTALL \
 NEWS \
 acinclude.m4 \
 bootstrap \
 version.h.in \
 \
 "$lone_dir"

# Formerly copied but actually generated or copied by ./bootstrap:
#   aclocal.m4 ltmain.sh
#   compile config.guess config.sub depcomp install-sh missing

copy_files xorriso/xorriso_bootstrap.txt    "$lone_dir"/bootstrap

copy_files xorriso/configure_ac.txt         "$lone_dir"/configure.ac

copy_files xorriso/xorriso_makefile_am.txt  "$lone_dir"/Makefile.am


# libisoburn

create_dir "$lone_dir"/libisoburn
copy_files \
 libisoburn/*.[ch] \
 "$lone_dir"/libisoburn
copy_files COPYRIGHT "$lone_dir"/libisoburn

touch \
 xorriso/man_1_xorriso.html \
 xorriso/man_1_xorrisofs.html \
 xorriso/man_1_xorrecord.html 

xorriso/convert_man_to_html.sh

create_dir "$lone_dir"/xorriso
copy_files \
 xorriso/xorriso.h \
 xorriso/xorriso_private.h \
 xorriso/sfile.h \
 xorriso/sfile.c \
 xorriso/aux_objects.h \
 xorriso/aux_objects.c \
 xorriso/findjob.h \
 xorriso/findjob.c \
 xorriso/check_media.h \
 xorriso/check_media.c \
 xorriso/misc_funct.h \
 xorriso/misc_funct.c \
 xorriso/text_io.h \
 xorriso/text_io.c \
 xorriso/match.h \
 xorriso/match.c \
 xorriso/emulators.h \
 xorriso/emulators.c \
 xorriso/disk_ops.h \
 xorriso/disk_ops.c \
 xorriso/cmp_update.h \
 xorriso/cmp_update.c \
 xorriso/parse_exec.h \
 xorriso/parse_exec.c \
 xorriso/opts_a_c.c \
 xorriso/opts_d_h.c \
 xorriso/opts_i_o.c \
 xorriso/opts_p_z.c \
 \
 xorriso/xorrisoburn.h \
 xorriso/base_obj.h \
 xorriso/base_obj.c \
 xorriso/lib_mgt.h \
 xorriso/lib_mgt.c \
 xorriso/sort_cmp.h \
 xorriso/sort_cmp.c \
 xorriso/drive_mgt.h \
 xorriso/drive_mgt.c \
 xorriso/iso_img.h \
 xorriso/iso_img.c \
 xorriso/iso_tree.h \
 xorriso/iso_tree.c \
 xorriso/iso_manip.h \
 xorriso/iso_manip.c \
 xorriso/write_run.h \
 xorriso/write_run.c \
 xorriso/read_run.h \
 xorriso/read_run.c \
 xorriso/filters.h \
 xorriso/filters.c \
 \
 xorriso/xorriso_main.c \
 xorriso/xorriso_timestamp.h \
 \
 xorriso/changelog.txt \
 xorriso/xorriso_eng.html \
 xorriso/make_docs.sh \
 xorriso/make_xorriso_standalone.sh \
 xorriso/unite_html_b_line.c \
 xorriso/convert_man_to_html.sh \
 xorriso/man_xorriso_to_html.sh \
 xorriso/man_xorrisofs_to_html.sh \
 xorriso/man_xorrecord_to_html.sh \
 xorriso/xorriso.texi \
 xorriso/xorriso.info \
 xorriso/xorrisofs.texi \
 xorriso/xorrisofs.info \
 xorriso/xorrecord.texi \
 xorriso/xorrecord.info \
 xorriso/xorriso-tcltk.texi \
 xorriso/xorriso-tcltk.info \
 xorriso/make_xorriso_1.c \
 xorriso/xorriso.1 \
 xorriso/xorrisofs.1 \
 xorriso/xorrecord.1 \
 xorriso/xorriso-tcltk.1 \
 xorriso/man_1_xorriso.html \
 xorriso/man_1_xorrisofs.html \
 xorriso/man_1_xorrecord.html \
 "$lone_dir"/xorriso
copy_files COPYRIGHT "$lone_dir"/xorriso

copy_files xorriso/xorriso_buildstamp_none.h \
           "$lone_dir"/xorriso/xorriso_buildstamp.h
copy_files xorriso/xorriso_buildstamp_none.h \
           "$lone_dir"/xorriso/xorriso_buildstamp_none.h

create_dir "$lone_dir"/doc
copy_files doc/partition_offset.wiki \
           doc/startup_file.txt \
           doc/qemu_xorriso.wiki \
           "$lone_dir"/doc

create_dir "$lone_dir"/test
copy_files \
 test/compare_file.c \
 test/merge_debian_isos \
 test/merge_debian_isos.texi \
 test/merge_debian_isos.info \
 test/merge_debian_isos.1 \
 "$lone_dir"/test

create_dir "$lone_dir"/frontend
copy_files \
 frontend/frontend_pipes_xorriso.c \
 frontend/README-tcltk \
 frontend/xorriso-tcltk \
 frontend/sh_on_named_pipes.sh \
 frontend/xorriso_broker.sh \
 frontend/grub-mkrescue-sed.sh \
 "$lone_dir"/frontend

create_dir "$lone_dir"/xorriso-dd-target
copy_files \
 xorriso-dd-target/xorriso-dd-target \
 xorriso-dd-target/xorriso-dd-target.texi \
 xorriso-dd-target/xorriso-dd-target.info \
 xorriso-dd-target/xorriso-dd-target.1 \
 "$lone_dir"/xorriso-dd-target


# releng

copy_tree releng "$lone_dir"/releng

rm "$lone_dir"/releng/auto_cxx
rm -r "$lone_dir"/releng/releng_generated_data
create_dir "$lone_dir"/releng/releng_generated_data


# nglibisofs

create_dir "$lone_dir"/libisofs
create_dir "$lone_dir"/libisofs/filters
goto_dir   "$current_dir"/nglibisofs-develop
copy_files libisofs/*.[ch] "$lone_dir"/libisofs
copy_files libisofs/filters/*.[ch] "$lone_dir"/libisofs/filters
copy_files doc/susp_aaip*.txt "$lone_dir"/doc
copy_files doc/zisofs_format.txt "$lone_dir"/doc
copy_files doc/zisofs2_format.txt "$lone_dir"/doc
copy_files doc/checksums.txt "$lone_dir"/doc
copy_files doc/boot_sectors.txt "$lone_dir"/doc
copy_files COPYRIGHT "$lone_dir"/libisofs
test -e CONTRIBUTORS && cat CONTRIBUTORS >>"$lone_dir"/CONTRIBUTORS

# To get a common version.h 
cat version.h.in >> "$lone_dir"/version.h.in


# libjte

create_dir "$lone_dir"/libjte
goto_dir   "$current_dir"/jte-develop
copy_files *.[ch] "$lone_dir"/libjte
copy_files COPYRIGHT "$lone_dir"/libjte
#
# Now using libisoburn/releng/jigdo-gen-md5-list because in jigit it is
# restricted to Linux and FreeBSD.
goto_dir   "$current_dir"/libisoburn-develop/releng
copy_files jigdo-gen-md5-list jigdo-gen-md5-list.1 "$lone_dir"/libjte


# libburn

create_dir "$lone_dir"/libburn
goto_dir   "$current_dir"/libburn-develop
copy_files libburn/*.[ch] "$lone_dir"/libburn
copy_files COPYRIGHT "$lone_dir"/libburn
cat CONTRIBUTORS >>"$lone_dir"/CONTRIBUTORS

# Delete a source module of yet unclear ancestry.
# The build process will avoid to use it.
rm "$lone_dir"/libburn/crc.c


# To get a common version.h 
cat version.h.in >> "$lone_dir"/version.h.in

# Decision about legal situation
goto_dir "$current_dir"/libisoburn-develop
if test "$create_gnu_xorriso" = "yes"
then
  copy_files xorriso/README_gnu_xorriso       "$lone_dir"/README
  copy_files xorriso/COPYRIGHT_gnu_xorriso    "$lone_dir"/COPYRIGHT
  copy_files xorriso/COPYING_gnu_xorriso      "$lone_dir"/COPYING
  copy_files xorriso/AUTHORS_gnu_xorriso      "$lone_dir"/AUTHORS

  # patch xorriso/xorriso.h to be GNU xorriso
  sed -e's/define Xorriso_libburnia_xorrisO/define Xorriso_GNU_xorrisO/' \
      -e's/This may be changed to Xorriso_GNU_xorrisO in order to c/C/' \
      <xorriso/xorriso.h  >"$lone_dir"/xorriso/xorriso.h

else

  copy_files README       "$lone_dir"/README

fi


# tarball

if test "$with_bootstrap_tarball" = 1
then

tarball_dir="$current_dir"/xorriso-"$xorriso_rev""$xorriso_pl"
mv "$lone_dir" "$tarball_dir"

goto_dir "$tarball_dir"

./bootstrap

# Remove unneeded temporary data from ./bootstrap
rm -r ./autom4te.cache

# Repair non-portable shell code output of ./bootstrap
(
 cd "$compile_dir" || exit 1
 sed -e 's/^for ac_header in$/test -z 1 \&\& for ac_header in dummy/' \
     < ./configure > ./configure-repaired
 if test "$?" = 0
 then
   echo "$0: Empty 'for ac_header in' found in configure." >&2
 fi
 mv ./configure-repaired ./configure
 chmod a+rx,go-w,u+w ./configure
)

if test "$create_gnu_xorriso" = "yes"
then
  # ftp-upload@gnu.org rejects Makefile.in with a dangerous chmod on make dist
  sed -e 's/-perm -777 -exec chmod a+rwx/-perm -755 -exec chmod u+rwx,go+rx/' \
      < Makefile.in  > new_Makefile.in
  
  mv new_Makefile.in Makefile.in
fi


cd "$current_dir"
tar czf ./xorriso-"$xorriso_rev""$xorriso_pl".tar.gz $(basename "$tarball_dir")

ls -l $(pwd)/xorriso-"$xorriso_rev""$xorriso_pl".tar.gz
        
mv "$tarball_dir" "$lone_dir"

fi

echo "Done"
echo "HINT: Now build xorriso/xorriso by:"
echo "        cd '$lone_dir' && ./configure && make"
echo
