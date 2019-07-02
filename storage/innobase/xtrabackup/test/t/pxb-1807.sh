#
# PXB-1807: xtrabackup does not accept fractional values for innodb_max_dirty_pages_pct
#

xtrabackup --innodb-max-dirty-pages-pct=34 --print-defaults 2>/dev/null | \
	   grep innodb-max-dirty-pages-pct > $topdir/pxb-1807.out

xtrabackup --innodb-max-dirty-pages-pct=12.34 --print-defaults 2>/dev/null | \
	   grep innodb-max-dirty-pages-pct >> $topdir/pxb-1807.out

diff -u $topdir/pxb-1807.out - <<EOF  || die "Unexpected output"
  --innodb-max-dirty-pages-pct=# 
innodb-max-dirty-pages-pct        34
  --innodb-max-dirty-pages-pct=# 
innodb-max-dirty-pages-pct        12.34
EOF
