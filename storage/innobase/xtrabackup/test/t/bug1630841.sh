#
# Bug 1630841: sha256 password doesn't work
#

require_server_version_higher_than 5.6.0

MYSQLD_EXTRA_MY_CNF_OPTS="
default-authentication-plugin=sha256_password
ssl-ca=${PWD}/inc/ssl-certs/cacert.pem
ssl-cert=${PWD}/inc/ssl-certs/server-cert.pem
ssl-key=${PWD}/inc/ssl-certs/server-key.pem
"

start_server

vlog "*************************************************************************"
cat $MYSQLD_ERRFILE
vlog "*************************************************************************"

vlog "Detectiong SSL support"

if has_openssl
then
	vlog "Server is built with OpenSSL"
	SEVER_SSL=openssl
else
	vlog "Server is built with YaSSL"
	SEVER_SSL=yassl
fi

if ${XB_BIN} --param 2>/dev/null | grep -q server-public-key-path
then
	vlog "Xtrabackup is built with OpenSSL"
	XTRABACKUP_SSL=openssl
else
	vlog "Xtrabackup is built with YaSSL"
	XTRABACKUP_SSL=yassl
fi

# using SSL encryption requires xtrabackup and mysql to use the same SSL library
if [ "$SEVER_SSL" = "$XTRABACKUP_SSL" ]
then

vlog "Creating user 'pxb'"

${MYSQL} ${MYSQL_ARGS} -e "CREATE USER pxb@'localhost' IDENTIFIED BY 'password1'"
${MYSQL} ${MYSQL_ARGS} -e "GRANT PROCESS, RELOAD, LOCK TABLES, REPLICATION CLIENT ON *.* TO pxb@'localhost' REQUIRE SSL"
${MYSQL} ${MYSQL_ARGS} -e "FLUSH PRIVILEGES"

vlog 'connecting with MYSQL cli'
run_cmd ${MYSQL} \
	--no-defaults \
	--user=pxb \
	--password=password1 \
	--host=127.0.0.1 \
	--port=${MYSQLD_PORT} \
	--ssl \
	--ssl-ca=${PWD}/inc/ssl-certs/cacert.pem \
	--ssl-cert=${PWD}/inc/ssl-certs/client-cert.pem \
	--ssl-key=${PWD}/inc/ssl-certs/client-key.pem \
	-e '\s'

vlog 'connecting with xtrabackup'
run_cmd ${XB_BIN} \
	--no-defaults \
	--backup \
	--user=pxb \
	--password=password1 \
	--host=127.0.0.1 \
	--port=${MYSQLD_PORT} \
	--ssl \
	--ssl-ca=${PWD}/inc/ssl-certs/cacert.pem \
	--ssl-cert=${PWD}/inc/ssl-certs/client-cert.pem \
	--ssl-key=${PWD}/inc/ssl-certs/client-key.pem \
	--target-dir=$topdir/backup1

fi

stop_server

vlog "*************************************************************************"
cat $MYSQLD_ERRFILE
vlog "*************************************************************************"

# sha256_password + asymmetric encryption requires OpenSSL
# both on server and xtrabackup

if [ "$SEVER_SSL" = "openssl" -a "$XTRABACKUP_SSL" = "openssl" ]
then

MYSQLD_EXTRA_MY_CNF_OPTS="
default-authentication-plugin=sha256_password
sha256_password_private_key_path=${PWD}/inc/ssl-certs/rsa_private_key.pem
sha256_password_public_key_path=${PWD}/inc/ssl-certs/rsa_public_key.pem
"

start_server

${MYSQL} ${MYSQL_ARGS} -e "CREATE USER pbx@'localhost' IDENTIFIED BY 'password1'"
${MYSQL} ${MYSQL_ARGS} -e "GRANT PROCESS, RELOAD, LOCK TABLES, REPLICATION CLIENT ON *.* TO pbx@'localhost'"
${MYSQL} ${MYSQL_ARGS} -e "FLUSH PRIVILEGES"

vlog 'connecting with MYSQL cli'
run_cmd ${MYSQL} \
	--no-defaults \
	--user=pbx \
	--password=password1 \
	--host=127.0.0.1 \
	--port=${MYSQLD_PORT} \
	-e '\s'

vlog 'connecting with xtrabackup'
run_cmd ${XB_BIN} \
	--no-defaults \
	--backup \
	--user=pbx \
	--password=password1 \
	--host=127.0.0.1 \
	--port=${MYSQLD_PORT} \
	--target-dir=$topdir/backup2

fi
