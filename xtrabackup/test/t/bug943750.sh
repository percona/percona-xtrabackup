################################################################################
# Bug #943750: innobackupex doesn't backup to nfs mount point
################################################################################

# Create a fake cp that will fail if '-p' is passed by innobackupex

cat >$topdir/cp <<"EOF"
#!/bin/bash

args=""
while (( "$#" ))
do
    if [ "$1" = "-p" ]
    then
        echo "'-p' passed as an argument to 'cp'!"
        exit 1
    fi
    args="$args $1"
    shift
done
/bin/cp $args
EOF

chmod +x $topdir/cp

PATH=$topdir:$PATH . inc/xb_local.sh
