#!/bin/sh
# $Id$

GIT_VERSION_FILE=GIT-VERSION-FILE
FIX_DEV=20120722-git

if [ -f "$GIT_VERSION_FILE" ]; then
	VN=`cat "$GIT_VERSION_FILE" 2> /dev/null`
	VC=`echo $VN | sed -e 's|^GIT_VERSION = \([0-9a-zA-Z\-\.]+\)|\1|;t;d;p'`
fi
if [ -z "$VC" ]; then
	if [ -d .git -o -f .git ]; then
		VN=`git log  --pretty=format:%cd_%t --date=short -1 HEAD 2> /dev/null`
		if [ -n "$VN" ]; then
			VC=`echo "$VN" | sed -e 's|-||g;s|_|-|g;'`
		fi
	fi
fi

# correct VC?
if [ -z "$VC" ]; then
	VC="$FIX_VER"
fi
VNN="GIT_VERSION_FILE = $VC"
if [ ! -f $GIT_VERSION_FILE -o "$VN" != "$VNN" ]; then
	echo "$VNN" > $GIT_VERSION_FILE
fi
