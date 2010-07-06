result=0
rm -rf results
mkdir results
for t in t/*.sh
do
   echo -n "$t      "
   $t > results/`basename $t`.out 2>&1
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
