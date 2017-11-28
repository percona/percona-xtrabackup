###############################################################################
# PXB-197: 5.6 MTR PXB failure in galera_sst_xtrabackup-v2-options on all nodes
###############################################################################

# Reproducible even on 'empty' server, so don't load any schema and data.
start_server

function check_pipestatus()
{
    pipestatus=${PIPESTATUS[@]}
    local command_index=0
    for i in ${pipestatus[@]}
    do
        ((command_index++))
        if [ $i -ne 0 ]
        then
            die "piped command #${command_index} died with error $i"
        fi
    done
}

key=12345678901234567890123456789012
algo=AES256

mkdir -p $topdir/tmp $topdir/tmp2 $topdir/tmp3
set -o pipefail

run_cmd xtrabackup --backup --no-version-check --stream=xbstream --target-dir=$topdir/backup \
    | xbcrypt --encrypt-algo=$algo --encrypt-key=$key \
    | xbcrypt -d --encrypt-algo=$algo --encrypt-key=$key \
    | xbstream -x -C $topdir/tmp

check_pipestatus

run_cmd xtrabackup --backup --no-version-check --stream=xbstream --target-dir=$topdir/backup \
    | xbcrypt --encrypt-algo=$algo --encrypt-key=$key --read-buffer-size=10K \
    | xbcrypt -d --encrypt-algo=$algo --encrypt-key=$key --read-buffer-size=10K \
    | xbstream -x -C $topdir/tmp2

check_pipestatus

run_cmd innobackupex --backup --no-version-check --stream=xbstream $topdir/backup2 \
    | xbcrypt --encrypt-algo=$algo --encrypt-key=$key \
    | xbcrypt -d --encrypt-algo=$algo --encrypt-key=$key \
    | xbstream -x -C $topdir/tmp3

check_pipestatus
