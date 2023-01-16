#
# PXB-1870: xtrabackup prepare get stuck if ib_sdi_get return DB_OUT_OF_MEMORY
#

start_server

mysql test <<EOF
CREATE TABLE user (
  id bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  cid int(11) NOT NULL DEFAULT '0',
  uid int(11) NOT NULL DEFAULT '0',
  gold int(11) NOT NULL DEFAULT '0',
  coin int(11) NOT NULL DEFAULT '0',
  price int(11) NOT NULL DEFAULT '0',
  time datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  type int(11) NOT NULL DEFAULT '0',
  chapterid int(11) NOT NULL DEFAULT '0',
  platformid int(11) NOT NULL,
  productlineid int(11) NOT NULL,
  PRIMARY KEY (id,uid)
) ENGINE=InnoDB
PARTITION BY HASH (uid) 
PARTITIONS 3000;
EOF

xtrabackup --backup --target-dir=$topdir/backup
xtrabackup --prepare --target-dir=$topdir/backup
