# Check lock-ddl, lock-ddl-timeout and lock-ddl-per-table

require_server_version_higher_than 5.7.0

start_server

load_sakila

mysql -e 'CREATE TABLE t_hello_你好(id INT, PRIMARY KEY(id)) ENGINE=InnoDB' test
mysql test <<EOF
CREATE TABLE R8EC8BBNEGLU37BCU93XW157W6NGEBL7I23VMK8YSTAVHFO0K8L3HD6K9L8N9CJ2 (
  a int(11) DEFAULT NULL
)
PARTITION BY RANGE (a)
(PARTITION OSU60ZGPEV3BJVZYP3BC3PPFX5TS82NJHT5FMIY1ZN1Y3GJAT4H5PET9T1SMCBCF VALUES LESS THAN (100),
 PARTITION J7QW390HPI2MYCNUA680XRHL5R5VX0YHMDT2T251MUM8QADJ6QKVSKVQOU0S9DKH VALUES LESS THAN (200),
 PARTITION H1AWLS9TM0LR3O1ATIKRVZKOCFBLYWWZVB7XO6VROR0DK2V89VHQ0TPCOJTSA40Z VALUES LESS THAN (300),
 PARTITION DMLQRPAEJ871QZDGQZ2M0TO9JU88ASTMDEH61SD7CGPXEV3T858EBZKL4MV5SQRD VALUES LESS THAN (400),
 PARTITION FULO5628LRMMP3DOEE6HNA2WQSLV3Q9MMWR3N498UPBC35BAUOYKJXNWOF1T5QRJ VALUES LESS THAN MAXVALUE);
EOF

mysql test -e 'INSERT INTO R8EC8BBNEGLU37BCU93XW157W6NGEBL7I23VMK8YSTAVHFO0K8L3HD6K9L8N9CJ2 (a) VALUES (1), (101), (201), (301), (401)'

mysql test <<EOF
/* fails in Jenkins with 'path is too long' CREATE DATABASE MC5NOGLQ9OFY7YM76Z1T758ZTPTJ6IPLVSLDHMSEXT63MLVHCPEW4DNU2OPQDRRE;
USE MC5NOGLQ9OFY7YM76Z1T758ZTPTJ6IPLVSLDHMSEXT63MLVHCPEW4DNU2OPQDRRE; */
CREATE TABLE p (a INT NOT NULL, b INT)
    PARTITION BY RANGE (a) PARTITIONS 3 SUBPARTITION BY KEY (b) (
        PARTITION O8W7066AGXADOMYWHT89TWMBJOMTFDMDC74WJ7IUPKD75LVU1ENOV1J008SJBKKF VALUES LESS THAN (200) (
            SUBPARTITION YWKQ987ZTKDJ33ZBMLW526153X86VXL4X44R15SPF8JQS92665MT0QI6BSNKAZY5,
            SUBPARTITION OSYA45V7KKPJ840E4CZ7CKFXDT3J1NNM8QTT9BQOF896CAZVWFY4K236VHYD1WXN,
            SUBPARTITION M6YGLCVSKPSVF1RYZA4XJNP7HP9P7OKBP0268T2HKJ0005BW3LLSALQJ94UE5ZSV),
        PARTITION F3HRUC798U6YIBQFSC9BDKOAUH2SD6B0A3IA7J4P2V8M5U84AAVCR27NNQGM8NI3 VALUES LESS THAN (600) (
            SUBPARTITION YKWIEPEMRMG097FJ0D8WBJC9TF93GW7GVSU8H0MXYSX940JYO0RNVR4W7YYEZDSF,
            SUBPARTITION WDJZ36D1IMDRB4ZABLZXWE7J00OP1WR028V1PREZN46PK9L3Y3ERVTWKXYTMC08W,
            SUBPARTITION KRR0PI4ZBU50X4YVWKDA65PFYRNB69EV5LMM3CMBUVUEIHN2MPY30O8J8WEOOG2Q),
        PARTITION UYZ1675BEXI942ED7EUYLK03GM90QG3ZGYBLBKGLNWVYNVYUZ70J78BJWYLGS6CV VALUES LESS THAN (1800) (
            SUBPARTITION XIYZDGAOHPZH1R6RTKURMSUENS8VIN1U1CL7T2594FWR1ELS55UDVDWVX65K3WPE,
            SUBPARTITION J64S0S338J2AV273XIHW1QHFRK9ZVLSIGSXFF4E22RYCLAG2J3H04PN6M70OB39Y,
            SUBPARTITION XRL60IN1WNLR4YQAIILJTB9XCDP7Z4CUXRCY9Y2ES55W6UQERG51QV1UYTMZW673))
EOF

function start_transaction() {
	run_cmd_expect_failure $MYSQL $MYSQL_ARGS <<EOF
BEGIN;
SELECT * FROM sakila.payment LIMIT 1;
SELECT SLEEP(10000);
EOF
}

function alter_table() {
	run_cmd_expect_failure $MYSQL $MYSQL_ARGS \
		-e "ALTER TABLE sakila.payment ADD COLUMN col1 INT"
}

if ! has_backup_locks ;
then

	run_cmd_expect_failure $XB_BIN $XB_ARGS \
		--backup --lock-ddl --target-dir=$topdir/backup1

else

	xtrabackup --backup --lock-ddl \
		--lock-ddl-timeout=2 --target-dir=$topdir/backup1

	start_transaction &
	tr_job_id=$!

	while ! mysql -e 'SHOW PROCESSLIST' | grep -q 'User sleep' ; do
		sleep 1
	done

	alter_table &
	ddl_job_id=$!

	while ! mysql -e 'SHOW PROCESSLIST' | grep -q 'Waiting for table metadata lock' ; do
		sleep 1
	done

	# SELECT blocks ALTER TABLE, ALTER TABLE blocks LOCK TABLES FOR BACKUP
	run_cmd_expect_failure \
		$XB_BIN $XB_ARGS --backup --lock-ddl --lock-ddl-timeout=2 \
				 --target-dir=$topdir/backup2

	mysql -Ne "SELECT CONCAT('KILL ', id, ';') FROM \
	INFORMATION_SCHEMA.PROCESSLIST WHERE info LIKE 'SELECT SLEEP%' \
	OR info LIKE 'ALTER TABLE%'" | mysql

	wait $tr_job_id
	wait $ddl_job_id

	xtrabackup --backup --lock-ddl \
		--lock-ddl-timeout=2 --target-dir=$topdir/backup3

fi

mysql -e "CREATE TABLE rcount (val INT)" test
mysql -e "INSERT INTO rcount (val) VALUES (0)" test

function heavy_index_rotation() {
	trap "finish=true" SIGUSR1
	finish="false"
	while [ $finish != "true" ] ; do
		mysql -e "CREATE INDEX idx_payment_date ON payment (payment_date)" sakila
		mysql -e "DROP INDEX idx_payment_date ON payment" sakila
		mysql -e "UPDATE rcount SET val = val + 1" test
	done
}

heavy_index_rotation &
job_id=$!

while [ `mysql test -Ne "SELECT val FROM rcount"` -lt "3" ] ; do
	sleep 1
done

xtrabackup --backup --lock-ddl-per-table --target-dir=$topdir/backup5
xtrabackup --prepare --target-dir=$topdir/backup5

if has_backup_locks ;
then
	xtrabackup --backup --lock-ddl --target-dir=$topdir/backup6
	xtrabackup --prepare --target-dir=$topdir/backup6
fi

# PXB-1539: lock-ddl-per-table reports error for table name with special characters
run_cmd_expect_failure grep 'failed to execute query SELECT \* FROM test.t_hello' $OUTFILE

kill -USR1 $job_id
wait $job_id

stop_server
