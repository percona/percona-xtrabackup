#!/bin/bash

XB_BUILD="autodetect"
while getopts "c:" options; do
	case $options in
	        c ) XB_BUILD="$OPTARG";;
	esac
done

CFLAGS=-g make testrun
./testrun -c $XB_BUILD
