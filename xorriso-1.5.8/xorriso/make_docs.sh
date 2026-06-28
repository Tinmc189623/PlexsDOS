#!/bin/sh
#
# Produce man page xorriso/xorriso.1 and info file xorriso/xorriso.info
# from base file xorriso/xorriso.texi.
# Same for xorriso/xorrisofs.texi and xorriso/xorrecord.texi.

( cd xorriso ; makeinfo --no-split ./xorriso.texi )
( cd xorriso ; makeinfo --no-split ./xorrisofs.texi )
( cd xorriso ; makeinfo --no-split ./xorrecord.texi )
( cd xorriso ; makeinfo --no-split ./xorriso-tcltk.texi )
( cd xorriso-dd-target ; makeinfo --no-split ./xorriso-dd-target.texi )
( cd test ; makeinfo --no-split ./merge_debian_isos.texi )

xorriso/make_xorriso_1 -auto
xorriso/make_xorriso_1 -auto -xorrisofs
xorriso/make_xorriso_1 -auto -xorrecord
xorriso/make_xorriso_1 -auto -xorriso-tcltk
xorriso/make_xorriso_1 -auto -xorriso-dd-target
xorriso/make_xorriso_1 -auto -merge_debian_isos


