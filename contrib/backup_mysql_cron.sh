#!/bin/bash
#
# Backup script for MySQL 5.5 on RHEL5 using XtraBackup
#
# Requires: xtrabackup, bash, awk, coreutils

## Settings ##
PATH="/bin:/usr/bin"
BACKUPDIR="/var/mysql/backups/xtrabackup"
MINFREE_M="5000"
INNOBACKUPEX_OPTIONS=""
RETENTION_DAYS="7"

## Logic ##
timestamp=`date +%Y%m%d_%H%M%S_%Z`
export PATH;

if [ ! -x "/usr/bin/innobackupex-1.5.1" ]; then
  echo "ERROR: /usr/bin/innobackupex-1.5.1 is not executable."
  exit 1
fi 

if [ ! -d ${BACKUPDIR} ]; then
  echo "ERROR: ${BACKUPDIR} is not a directory."
  exit 2
fi

freespace_m=`df -k ${BACKUPDIR} | awk '{ if ($4 ~ /^[0-9]*$/) { print int($4/1024) } }'`
if [ ${freespace_m} -le ${MINFREE_M} ]; then
  echo "ERROR: There is less than ${MINFREE_M} MB of free space on ${BACKUPDIR}"
  exit 3
fi

# Remove backups older than $RETENTION_DAYS
find /var/mysql/backups/xtrabackup -name "*_*_*_xtrabackup\.tar\.bz2" -type f -mtime +${RETENTION_DAYS}

# Create the backup
/usr/bin/innobackupex-1.5.1 ${INNOBACKUPEX_OPTIONS} --stream=tar /tmp | bzip2 > ${BACKUPDIR}/${timestamp}_xtrabackup.tar.bz2
