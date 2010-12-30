#!/bin/bash

result=0
rm -rf results
mkdir results
function usage()
{
	echo "Usage: $0 [-m mysql_version] [-g] [-h]"
	echo "-m version  MySQL version to use. Possible values: system, 5.0, 5.1, 5.5, percona. Default is system"
	echo "-g          Output debug information to results/*.out"
	echo "-t          Run only a single named test"
	echo "-h          Print this help megssage"
}
XTRACE_OPTION=""
export SKIPPED_EXIT_CODE=200
export MYSQL_VERSION="system"
while getopts "gh?m:t:" options; do
	case $options in
		m ) export MYSQL_VERSION="$OPTARG";;
		t ) tname="$OPTARG";;
		g ) XTRACE_OPTION="-x";;
		h ) usage; exit;;
		\? ) usage; exit -1;;
		* ) usage; exit -1;;
	esac
done

if [ -n "$tname" ]
then
   tests="$tname"
else
   tests="t/*.sh"
fi

for t in $tests
do
   printf "%-40s" $t
   bash $XTRACE_OPTION $t > results/`basename $t`.out 2>&1
   rc=$?
   if [ $rc -eq 0 ]
   then
       echo "[passed]"
   elif [ $rc -eq $SKIPPED_EXIT_CODE ]
   then
       echo "[skipped]"
   else
       echo "[failed]"
       result=1
   fi
done

if [ $result -eq 1 ]
then
    echo "There are failing tests!!!"
    echo "See results/ for detailed output"
    exit -1
fi
