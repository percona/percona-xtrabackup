###############################################################################
# Bug #1418438: innobackupex --compress only compress innodb tables
###############################################################################

start_server

mysql -e "CREATE TABLE test (A INT PRIMARY KEY) ENGINE=MyISAM" test

innobackupex --compress --no-timestamp --include=test.test $topdir/backup

diff -u <(LANG=C ls $topdir/backup/test) - <<EOF
test.MYD.qp
test.MYI.qp
test.frm.qp
EOF
