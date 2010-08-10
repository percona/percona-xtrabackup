result=0
rm -rf results
mkdir results
function usage()
{
	echo "Usage: $0 [-g] [-h]"
	echo "-g	Output debug information to results/*.out"
	echo "-h	Print this help megssage"
}
XTRACE_OPTION=""
while getopts "gh?" options; do
	case $options in
		g ) XTRACE_OPTION="-x";;
		h ) usage; exit;;
		\? ) usage; exit -1;;
		* ) usage; exit -1;;
	esac
done
for t in t/*.sh
do
   echo -n "$t      "
   bash $XTRACE_OPTION $t > results/`basename $t`.out 2>&1
   #$t 
   if [ $? -eq 0 ]
   then
       echo "[passed]"
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
