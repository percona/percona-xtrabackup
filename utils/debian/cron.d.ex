#
# Regular cron jobs for the xtrabackup package
#
0 4	* * *	root	[ -x /usr/bin/xtrabackup_maintenance ] && /usr/bin/xtrabackup_maintenance
