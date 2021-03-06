#!/bin/sh
# Copyright (C) 2008-2011 Red Hat, Inc. All rights reserved.
#
# This copyrighted material is made available to anyone wishing to use,
# modify, copy, or redistribute it subject to the terms and conditions
# of the GNU General Public License v.2.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# 'Test for proper escaping of strings in metadata (bz431474)'

. lib/test

aux prepare_devs 2
aux lvmconf 'devices/global_filter = [ "a|.*LVMTEST.*dev/mapper/.*pv[0-9_]*$|", "r|.*|" ]'

# for udev impossible to create
pv_ugly="__\"!@#\$%^&*,()|@||'\\\"__pv1"

# 'set up temp files, loopback devices'
name=$(basename "$dev1")
dmsetup rename "$name" "$PREFIX$pv_ugly"
dev1=$(dirname "$dev1")/"$PREFIX$pv_ugly"

dm_table | grep -F "$pv_ugly"

# 'pvcreate, vgcreate on filename with backslashed chars'
created="$dev1"
# when used with real udev without fallback, it will fail here
pvcreate "$dev1" || created="$dev2"
pvdisplay | should grep -F "$pv_ugly"
should check pv_field "$dev1" pv_name "$dev1"
vgcreate $vg "$created"
# 'no parse errors and VG really exists'
vgs $vg 2>err
not grep "Parse error" err
