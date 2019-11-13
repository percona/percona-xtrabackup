#
# PXB-1953: crash while rolling back transacions affecting mediumtext indexes
#

start_server

mysql test <<EOF
CREATE TABLE tt_3_p (
  v0 varchar(18) DEFAULT NULL,
  c1 char(16) DEFAULT NULL,
  c2 char(17) DEFAULT NULL,
  mt3 mediumtext,
  v4 varchar(27) DEFAULT NULL,
  i5 int(11) NOT NULL AUTO_INCREMENT,
  KEY tt_3_pi0 (mt3(27),c1,v0,i5),
  KEY tt_3_pi1 (mt3(6),c1),
  KEY tt_3_pi2 (i5,c1),
  KEY tt_3_pi3 (v4,mt3(23))
) ENGINE=InnoDB
AUTO_INCREMENT=99916
DEFAULT CHARSET=utf8mb4
COLLATE=utf8mb4_0900_ai_ci
ROW_FORMAT=COMPRESSED;
EOF

mysql test < inc/pxb-1953.sql

cat >$topdir/sql <<EOF
START TRANSACTION;
EOF

for i in {500..600} ; do
    echo "DELETE FROM tt_3_p WHERE i5 = $i;"
done | cat >>$topdir/sql

cat >>$topdir/sql <<EOF
SELECT SLEEP (10000);
EOF

mysql test < $topdir/sql &

while ! mysql -e 'SHOW PROCESSLIST' | grep -q 'User sleep' ; do
	sleep 1
done

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup
