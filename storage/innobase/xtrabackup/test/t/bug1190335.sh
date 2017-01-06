###############################################################################
# Bug #1190335: Stream decryption fails with options in my.cnf
###############################################################################

XB_EXTRA_MY_CNF_OPTS="
loose-encrypt=AES256
loose-encrypt-key=6F3AD9F428143F133FD7D50D77D91EA4
"

start_server

innobackupex --stream=xbstream $topdir/tmp | xbstream -xv -C $topdir/tmp
diff -u <(LANG=C ls $topdir/tmp/test) - <<EOF
db.opt.xbcrypt
EOF
