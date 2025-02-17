. inc/common.sh

start_server 

vlog "check if cfg version is 6 in table"

mkfifo $topdir/fifo

$MYSQL $MYSQL_ARGS <$topdir/fifo &

client_pid=$!

# Open the pipe for writing. This is required to prevent cat from closing the
# pipe when stdout is redirected to it

exec 3>$topdir/fifo

cat >&3 <<EOF
USE test;
CREATE TABLE t1(a INT) ENGINE=InnoDB;
FLUSH TABLE t1 FOR EXPORT;
EOF


# Let the client complete the above set of statements
vlog "waiting for 3 seconds to ensure cfg file is generated"
sleep 3

CFG_VERSION=`od -j3 -N1 -t x1 -An $mysql_datadir/test/t1.cfg  | head -30 | tr -d ' ' | tr -d '\n'`
CFG_VERSION_B=$(xxd -b -l4 -s0 $mysql_datadir/test/t1.cfg | awk '{print $2 $3 $4 $5}')
CFG_VERSION=$((2#$CFG_VERSION_B))

if [ "$CFG_VERSION" -le "8" ]; then
	vlog "version of CFG is $CFG_VERSION"
else
	vlog "PXB should have changed the source code to handle the new version $CFG_VERSION"
	exit -1
fi

# Terminate the background client
echo "exit" >&3
exec 3>&-
wait $client_pid


