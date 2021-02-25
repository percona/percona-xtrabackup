#!/bin/bash

#
# PXB-2371: --Vulnerability: Path traversal 
#

. inc/common.sh

my_dir=$TMPDIR
file="$my_dir/a"
dir_dst="$my_dir/dst"
file_xb="$my_dir/test.xb"

# Test 1: Creating without -P removes the absolute path prefix

## Set up
rm -rf $file $file_xb $dir_dst
mkdir -p $dir_dst
echo "a" > $file

## Run
run_cmd xbstream -v -c $file > $file_xb
rm -f $file
mkdir -p $dir_dst/$(dirname $file)  # xbstream doesn't create parent directories
run_cmd xbstream -v -x -C $dir_dst < $file_xb

## Check: The file was extracted in the relative path
run_cmd_expect_failure ls -l $file
run_cmd ls -l $dir_dst/$file

## Cleanup
rm -rf $file $file_xb $dir_dst

# Test 2: Creating with -P preserves the absolute path

## Set up
rm -rf $file $file_xb $dir_dst
mkdir -p $dir_dst
echo "a" > $file

## Run
run_cmd xbstream -v -c $file -P > $file_xb
rm -f $file
run_cmd xbstream -v -x -C $dir_dst -P < $file_xb

## Check: The file was extracted in the absolute path
run_cmd ls -l $file
run_cmd_expect_failure ls -l $dir_dst/$file

## Cleanup
rm -rf $file $file_xb $dir_dst

# Test 3: Extracting a stream with an absolute path file
# without -P is an error
 
## Set up
rm -rf $file $file_xb $dir_dst
mkdir -p $dir_dst
echo "a" > $file

## Run
run_cmd xbstream -v -c $file -P > $file_xb
rm -f $file
run_cmd_expect_failure xbstream -v -x -C $dir_dst < $file_xb

## Cleanup
rm -rf $file $file_xb $dir_dst
