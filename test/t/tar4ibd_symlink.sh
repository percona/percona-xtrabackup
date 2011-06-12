########################################################################
# Tests for tar4ibd + symlinks (bug #387587)
########################################################################

. inc/common.sh

init
run_mysqld --innodb_file_per_table
load_dbase_schema sakila
load_dbase_data sakila

# Copy some .ibd files to a temporary location and replace them with symlinks

mv $topdir/mysql/sakila/actor.ibd $topdir
ln -s $topdir/actor.ibd $topdir/mysql/sakila

mv $topdir/mysql/sakila/customer.ibd $topdir
ln -s $topdir/customer.ibd $topdir/customer_link.ibd
ln -s $topdir/customer_link.ibd $topdir/mysql/sakila/customer.ibd

# Take backup
mkdir -p $topdir/backup
run_cmd ${IB_BIN} --socket=$mysql_socket --stream=tar $topdir/backup > $topdir/backup/out.tar 2> $OUTFILE

stop_mysqld

# Remove datadir
rm -r $mysql_datadir

# Remove the temporary files referenced by symlinks
rm -f $topdir/actor.ibd
rm -f $topdir/customer.ibd
rm -f $topdir/customer_link.ibd

# Restore sakila
vlog "Applying log"
backup_dir=$topdir/backup
cd $backup_dir
$TAR -ixvf out.tar
cd - >/dev/null 2>&1 

run_cmd ${IB_BIN} --apply-log $backup_dir >> $OUTFILE 2>&1

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir

run_cmd ${IB_BIN} --copy-back $backup_dir >> $OUTFILE 2>&1

run_mysqld

# Check sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT COUNT(*) FROM actor" sakila
run_cmd ${MYSQL} ${MYSQL_ARGS} -e "SELECT COUNT(*) FROM customer" sakila
stop_mysqld
clean
