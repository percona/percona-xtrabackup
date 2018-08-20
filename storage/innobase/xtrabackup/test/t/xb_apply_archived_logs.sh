. inc/common.sh

skip_test "Enable when archived logs functionality restored"

#The result to return
RESULT=0

function repeat_until_new_arch_log_created
{
	local arch_log_dir=$1
	local command=$2
	local stop_lsn=`run_cmd $MYSQL $MYSQL_ARGS test -e 'SHOW ENGINE INNODB STATUS\G'|grep 'Log sequence number'|awk '{print $4}'`
	local old_arch_logs_count=`ls -al $arch_log_dir/| wc -l`
	local new_arch_logs_count=0

	echo $old_arch_logs_count
	# To be sure the data was flushed to archived log wait until new file
	# with START_LSN > CURRENT_LSN is created
	local max_lsn=0;
	while [ $max_lsn -le $stop_lsn ];
	do
		$command
		for i in $arch_log_dir/*;
		do
			local lsn=${i#$arch_log_dir/ib_log_archive_}
			if [ $lsn -gt $max_lsn ];
			then
				max_lsn=$lsn
			fi
		done
	done
}

function check_if_equal
{
	local NAME=$1
	local VAL1=$2
	local VAL2=$3
	local MSG="$NAME ($VAL1 == $VAL2):"
	if [ $VAL1 -ne $VAL2 ]
	then
		vlog "$MSG failed"
		RESULT=-1
	else
		vlog "$MSG passed"
	fi
}

function check_if_not_equal
{
	local NAME=$1
	local VAL1=$2
	local VAL2=$3
	local MSG="$NAME ($VAL1 != $VAL2):"
	if [ $VAL1 -eq $VAL2 ]
	then
		vlog "$MSG failed"
		RESULT=-1
	else
		vlog "$MSG passed"
	fi
}

function fill_tables
{
	local TABLES_COUNT=$1
	local TABLE_NAME=$2

	for i in `seq 1 $TABLES_COUNT`; do
		local TN=$TABLE_NAME$i
		run_cmd $MYSQL $MYSQL_ARGS test \
<<EOF
		INSERT INTO $TN (B)
			SELECT ${TN}_1.B FROM
				$TN ${TN}_1,
				$TN ${TN}_2,
				$TN ${TN}_3
				LIMIT 10000;

EOF
	done
}

function create_and_fill_db
{
	local TABLES_COUNT=$1
	local TABLE_NAME=$2
	local ARCH_LOG_DIR=$3
	local CREATE_TABLE_OPTIONS=${4:-''}

	for i in `seq 1 $TABLES_COUNT`; do
		local TN=$TABLE_NAME$i

		run_cmd $MYSQL $MYSQL_ARGS test \
<<EOF
		CREATE TABLE $TN
			(A INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
			 B INT(10) UNSIGNED NOT NULL DEFAULT 0) ENGINE=INNODB $CREATE_TABLE_OPTIONS;
		INSERT INTO $TN (B) VALUES (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1),
					   (1),(1),(1),(1),(1),(1),(1),(1),(1),(1);
EOF
	done

	repeat_until_new_arch_log_created \
		$ARCH_LOG_DIR \
		"fill_tables $TABLES_COUNT $TABLE_NAME"
}

function make_changes
{
	local START_I=$1
	local TABLES_COUNT=$2
	local TABLE_NAME=$3
	local CLIENT_CMD="${MYSQL} ${MYSQL_ARGS} -Ns test -e"

	for i in `seq $START_I $(($START_I+$TABLES_COUNT-1))`; do
		local TN=$TABLE_NAME$(($i-$START_I+1))

		#INSERT
		I_FROM[$i]=`$CLIENT_CMD "select max(a) from $TN"`
		$CLIENT_CMD "insert into $TN (b) select b from $TN limit 1000"
		I_TO[$i]=`$CLIENT_CMD "select max(a) from $TN"`
		vlog "Inserted rows for $i are in the range [${I_FROM[$i]}, ${I_TO[$i]})"
		I_COUNT[$i]=`$CLIENT_CMD "select count(*) from $TN where a >= ${I_FROM[$i]} and a < ${I_TO[$i]} "`

		#DELETE
		if [ $i -gt $TABLES_COUNT ];
		then
			local START_ROW=${U_TO[$(($i-$TABLES_COUNT))]}
		else
			local START_ROW=1000
		fi
		D_FROM[$i]=`$CLIENT_CMD "select min(a) from (select a from $TN where a >= $START_ROW limit 1000) as temp_table"`
		D_TO[$i]=`$CLIENT_CMD "select max(a) from (select a from $TN where a >= $START_ROW limit 1000) as temp_table"`
		D_COUNT[$i]=`$CLIENT_CMD "select count(*) from $TN where a>=${D_FROM[$i]} and a<${D_TO[$i]}"`
		$CLIENT_CMD "delete from $TN where a >= ${D_FROM[$i]} and a < ${D_TO[$i]}"
		vlog "Deleted rows for $i are in the range [${D_FROM[$i]}, ${D_TO[$i]}), total ${D_COUNT[$i]} are deleted"

		#UPDATE
		U_FROM[$i]=${D_TO[$i]}
		U_TO[$i]=`$CLIENT_CMD "select max(a) from (select a from $TN where a >= ${U_FROM[$i]} limit 1000) as temp_table"`
		U_COUNT[$i]=`$CLIENT_CMD "select count(*) from $TN where a>=${U_FROM[$i]} and a<${U_TO[$i]}"`
		$CLIENT_CMD "update $TN set b=2 where a >= ${U_FROM[$i]} and a < ${U_TO[$i]}"
		vlog "Updated rows for $i are in the range [${U_FROM[$i]}, ${U_TO[$i]}), total ${U_COUNT[$i]} are updated"

	done
}

function get_changes
{
	local START_I=$1
	local TABLES_COUNT=$2
	local TABLE_NAME=$3
	local CLIENT_CMD="${MYSQL} ${MYSQL_ARGS} -Ns test -e"

	for i in `seq $START_I $(($START_I+$TABLES_COUNT-1))`; do
		local TN=$TABLE_NAME$(($i-$START_I+1))

		#INSERT
		I_COUNT1[$i]=`$CLIENT_CMD "select count(*) from $TN where a >= ${I_FROM[$i]} and a < ${I_TO[$i]} "`
		#DELETE
		D_COUNT1[$i]=`$CLIENT_CMD "select count(*) from $TN where a >= ${D_FROM[$i]} and a < ${D_TO[$i]} "`
		#UPDATE
		U_COUNT1[$i]=`$CLIENT_CMD "select count(*) from $TN where a >= ${U_FROM[$i]} and a < ${U_TO[$i]} and b = 2"`
	done 
}

function check_changes
{
	local START_I=$1
	local TABLES_COUNT=$2
	local CMD=$3

	for i in `seq $START_I $(($START_I+$TABLES_COUNT-1))`; do
		$CMD "INSERT TEST for $i" ${I_COUNT[$i]} ${I_COUNT1[$i]}
		$CMD "DELETE TEST for $i" 0 ${D_COUNT1[$i]}
		$CMD "UPDATE TEST for $i" ${U_COUNT[$i]} ${U_COUNT1[$i]}
	done
}

function unset_global_variables
{
	unset I_FROM
	unset I_TO
	unset I_COUNT

	unset D_FROM
	unset D_TO
	unset D_COUNT

	unset U_FROM
	unset U_TO
	unset U_COUNT

	unset I_FROM1
	unset I_TO1
	unset I_COUNT1

	unset D_FROM1
	unset D_TO1
	unset D_COUNT1

	unset U_FROM1
	unset U_TO1
	unset U_COUNT1
}

function test_archived_logs
{
	local TABLE_NAME=T
	local TABLES_COUNT=4
	local EXTRA_OPTIONS=${1:-''}
	local CREATE_TABLE_OPTIONS=${2:-''}

	#Setup server environment to get access to some variables
	init_server_variables 1
	switch_server 1
	local BASE_BACKUP_DIR=$topdir/backup_base
	local BACKUP_DIR=$topdir/backup
	local BASE_DATA_DIR=$topdir/base_data
	local ARCHIVED_LOGS_DIR=$topdir/archived_logs
	local XTRABACKUP_OPTIONS="--innodb_log_file_size=2M $EXTRA_OPTIONS"
	#Setup ROW binlog format to supress warnings in result file
	local SERVER_OPTIONS="$XTRABACKUP_OPTIONS --innodb_log_archive=ON --innodb_log_arch_dir=$ARCHIVED_LOGS_DIR  --binlog-format=ROW"
	mkdir -p $BASE_BACKUP_DIR $BACKUP_DIR
	mkdir -p $ARCHIVED_LOGS_DIR
	reset_server_variables 1

	###################################################################
	# --to-lsn test. It checks the availability to apply logs only to #
	# the certain LSN.                                                #
	###################################################################
	start_server $SERVER_OPTIONS
	#Create and fill tables to generate log files
	create_and_fill_db $TABLES_COUNT $TABLE_NAME $ARCHIVED_LOGS_DIR $CREATE_TABLE_OPTIONS
	#Backup the data
	xtrabackup --backup --datadir=$mysql_datadir --target-dir=$BASE_BACKUP_DIR $XTRABACKUP_OPTIONS
	#Make some changes in tables after backup is done
	make_changes 1 $TABLES_COUNT $TABLE_NAME
	#Make sure that changes are flushed to archived log
	repeat_until_new_arch_log_created \
		$ARCHIVED_LOGS_DIR \
		"fill_tables $TABLES_COUNT $TABLE_NAME"
	#Remember current LSN
	local LSN=`run_cmd $MYSQL $MYSQL_ARGS test -e 'SHOW ENGINE INNODB STATUS\G'|grep 'Log sequence number'|awk '{print $4}'`
	#Make more changes over remembered LSN
	make_changes $(($TABLES_COUNT+1)) $TABLES_COUNT $TABLE_NAME
	#Make sure the above changes are flushed to archived log
	repeat_until_new_arch_log_created \
		$ARCHIVED_LOGS_DIR \
		"fill_tables $TABLES_COUNT $TABLE_NAME"
	stop_server
	cp -R $mysql_datadir $BASE_DATA_DIR

	#########################################
	# Apply logs only to the remembered lsn #
	#########################################
	# --apply-log-only is set implicitly because unfinished transactions
	# can be finished on further logs applying but using this option with
	# --innodb-log-arch-dir is tested here to prove this bug
	# https://bugs.launchpad.net/percona-xtrabackup/+bug/1199555 is not
	# concerned with this case
	cp -R $BASE_BACKUP_DIR/* $BACKUP_DIR
	xtrabackup --prepare \
		   --target-dir=$BACKUP_DIR \
		   --innodb-log-arch-dir=$ARCHIVED_LOGS_DIR \
		   --to-archived-lsn=$LSN \
		   --apply-log-only \
		   $XTRABACKUP_OPTIONS
	#Copy prepared data to server data dir
	cp -R $BACKUP_DIR/* $mysql_datadir
	rm $mysql_datadir/ib_*
	#Start server with prepared data
	start_server "--binlog-format=ROW $EXTRA_OPTIONS"
	#Get values from restored data files before remembered LSN
	get_changes 1 $TABLES_COUNT $TABLE_NAME
	#Get values from restored data files after remembered LSN
	get_changes $(($TABLES_COUNT+1)) $TABLES_COUNT $TABLE_NAME
	#We don't need server already
	stop_server
	#Check if the changes which was made before remembered LSN are in the
	#restored databse
	check_changes 1 $TABLES_COUNT 'check_if_equal'
	#Check if the changes which was made after remembered LSN are NOT in the
	#restored databse
	check_changes $(($TABLES_COUNT+1)) $TABLES_COUNT 'check_if_not_equal'
	# Apply the rest of archived logs
	xtrabackup --prepare \
		   --target-dir=$BACKUP_DIR \
		   --innodb-log-arch-dir=$ARCHIVED_LOGS_DIR \
		   --apply-log-only \
		   $XTRABACKUP_OPTIONS
	#Copy prepared data to server data dir
	cp -R $BACKUP_DIR/* $mysql_datadir
	rm $mysql_datadir/ib_*
	#Start server with prepared data
	start_server "--binlog-format=ROW $EXTRA_OPTIONS"
	#Get values from restored data files before remembered LSN
	get_changes 1 $TABLES_COUNT $TABLE_NAME
	#Get values from restored data files after remembered LSN
	get_changes $(($TABLES_COUNT+1)) $TABLES_COUNT $TABLE_NAME
	stop_server
	#Check if the changes which was made before remembered LSN are in the
	#restored databse
	check_changes 1 $TABLES_COUNT 'check_if_equal'
	#Check if the changes which was made after remembered LSN are in the
	#restored databse
	check_changes $(($TABLES_COUNT+1)) $TABLES_COUNT 'check_if_equal'
	rm -rf $mysql_datadir
	rm -rf $BACKUP_DIR

	##################################################
	# Check the possibility of applying logs by sets #
	##################################################
	cp -R $BASE_DATA_DIR $mysql_datadir
	cp -R $BASE_BACKUP_DIR $BACKUP_DIR
	mkdir -p $ARCHIVED_LOGS_DIR/1 $ARCHIVED_LOGS_DIR/2
	#Make two log files sets. The first set contains the first two files,
	#the second set contains the rest and the last log file from the first
	#set.
	pushd .
	cd $ARCHIVED_LOGS_DIR
	for i in *;
	do
		test -f $i || continue
		local n=${i#ib_log_archive_}
		if [ $n -le $LSN ];
		then
			mv $i 1/;
		else
			mv $i 2/;
		fi;
	done
	cd 1
	find . -type f -printf "%T+ %p\n" | cut -d' ' -f2 | sort -n | tail -1 | \
		xargs -I{} cp {} ../2/
	popd
	#Prepare the first set
	xtrabackup --prepare \
		   --target-dir=$BACKUP_DIR \
		   --innodb-log-arch-dir=$ARCHIVED_LOGS_DIR/1 \
		   $XTRABACKUP_OPTIONS
	#Prepare the second set
	xtrabackup --prepare \
		   --target-dir=$BACKUP_DIR \
		   --innodb-log-arch-dir=$ARCHIVED_LOGS_DIR/2 \
		   $XTRABACKUP_OPTIONS
	#Copy prepared data to server data dir
	cp -R $BACKUP_DIR/* $mysql_datadir
	rm $mysql_datadir/ib_*
	#Start server with prepared data
	start_server "--binlog-format=ROW $EXTRA_OPTIONS"
	#Get values from restored data files before remembered LSN
	get_changes 1 $TABLES_COUNT $TABLE_NAME
	#Get values from restored data files after remembered LSN
	get_changes $(($TABLES_COUNT+1)) $TABLES_COUNT $TABLE_NAME
	stop_server
	#Check all made changes
	check_changes 1 $TABLES_COUNT 'check_if_equal'
	check_changes $(($TABLES_COUNT+1)) $TABLES_COUNT 'check_if_equal'

	#Clean up dir for the next procedure launch
	rm -rf $BACKUP_DIR/* $BASE_BACKUP_DIR/*
	rm -rf $mysql_datadir $BASE_DATA_DIR
	rm -rf $ARCHIVED_LOGS_DIR/*

	#Clean up variables
	unset_global_variables
}

require_xtradb
require_server_version_higher_than '5.6.10'

test_archived_logs
test_archived_logs '' 'ROW_FORMAT=COMPRESSED'

exit $RESULT
