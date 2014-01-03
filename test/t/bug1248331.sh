########################################################################
# Bug #1248331: innobackupex error with ANSI_QUOTES sql_mode
########################################################################

ib_inc_extra_args=

MYSQLD_EXTRA_MY_CNF_OPTS="
sql_mode=\"ANSI_QUOTES\"
"

. inc/ib_incremental_common.sh
