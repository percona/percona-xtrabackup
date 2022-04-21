#!/bin/bash

#
# PXB-2422: --Bug: Extract sub-directories
#

. inc/common.sh

dir_src="$TMPDIR/subdir"
file="$dir_src/a.txt"
dir_dst="$TMPDIR/dst"
file_xb="$TMPDIR/test.xb"

# Test 1: xbstream extracts files in sub-directories

## Set up
rm -rf $dir_src $dir_dst $file_xb
mkdir -p $dir_src
echo "a" > $file
mkdir -p $dir_dst

## Run
run_cmd xbstream -v -c $file > $file_xb
rm -rf $dir_src
run_cmd xbstream -v -x -C $dir_dst < $file_xb

## Check
run_cmd_expect_failure ls -l $file
run_cmd ls -l $dir_dst/$file

## Cleanup
rm -rf $dir_src $dir_dst $file_xb
