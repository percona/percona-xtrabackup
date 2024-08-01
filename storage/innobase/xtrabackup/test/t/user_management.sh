#
# PXB-2644 - Cannot perform backup with user created with REQUIRE x509
#


########################################################################
# Creates an user and perform a backup with the newly created user.
# Expects up to 2 arguments:
# - $1 is the GRANT option (eg.: IDENTIFIED WITH x BY y ).
# - $2 is the options passed to xtrabackup (eg.: --password=x ).
########################################################################
function create_user_and_take_backup()
{
  GRANT_ARGS=$1
  BACKUP_ARGS=${2:-""}
  mysql -e "CREATE USER pxb@localhost ${GRANT_ARGS}"
  mysql -e "GRANT ALL ON *.* TO pxb@localhost"

  xtrabackup -u pxb --backup --target-dir=${topdir}/backup ${BACKUP_ARGS}

  #Clean up
  mysql -e "DROP USER pxb@localhost"
  rm -rf ${topdir}/backup
}
start_server
PASSWORD="SomeStr0ngPwD!"

vlog "Test 1 - caching_sha2_password"
create_user_and_take_backup \
"IDENTIFIED WITH 'caching_sha2_password' BY '${PASSWORD}'" \
"--password=${PASSWORD}"

vlog "Test 2 - REQUIRE X509 via TCP"
create_user_and_take_backup \
"IDENTIFIED BY '${PASSWORD}' REQUIRE x509" \
"--password=${PASSWORD} --host=127.0.0.1 --port=${MYSQLD_PORT} --ssl-key=${mysql_datadir}/client-key.pem --ssl-cert=${mysql_datadir}/client-cert.pem --ssl-ca=${mysql_datadir}/ca.pem"


# Without sslopt-case.h socket connections over TCP won't work
vlog "Test 3 - REQUIRE X509 via socket"
create_user_and_take_backup \
"IDENTIFIED BY '${PASSWORD}' REQUIRE x509" \
"--password=${PASSWORD} --socket=${MYSQLD_SOCKET} --ssl-key=${mysql_datadir}/client-key.pem --ssl-cert=${mysql_datadir}/client-cert.pem --ssl-ca=${mysql_datadir}/ca.pem"
