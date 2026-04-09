##########################################################################
# PXB-3427 : Parallelize incremental delta apply in incremental backup prepare
##########################################################################

. inc/common.sh

start_server --innodb_file_per_table --innodb_buffer_pool-size=1G --innodb_redo_log_capacity=2G

num_tables=1000
num_threads=16
batch_size=$((num_tables / num_threads))
pids=()

function create_stored_proc() {
 local dbname=${1:-test};
 mysql "$dbname" << EOF
DELIMITER $$

CREATE PROCEDURE create_table_range(IN start_idx INT, IN end_idx INT)
BEGIN
    DECLARE i INT;
    SET i = start_idx;

    WHILE i <= end_idx DO
        SET @query = CONCAT('CREATE TABLE t', i, ' (
            id INT PRIMARY KEY AUTO_INCREMENT,
            name CHAR(255) DEFAULT ''DefaultName'',
            address CHAR(255) DEFAULT ''DefaultAddress'',
            city CHAR(255) DEFAULT ''DefaultCity'',
            country CHAR(255) DEFAULT ''DefaultCountry'',
            description CHAR(255) DEFAULT ''DefaultDescription''
        )');
        PREPARE stmt FROM @query;
        EXECUTE stmt;
        DEALLOCATE PREPARE stmt;
        SET i = i + 1;
    END WHILE;
END$$

DELIMITER ;

EOF
}

function drop_stored_proc() {
local dbname=${1:-test};
mysql -e "DROP PROCEDURE create_table_range" "$dbname"
}

create_tables_parallel() {
    local start_idx=$1
    local end_idx=$2
    local dbname=${3:-test}  # Use third parameter if provided, otherwise default to 'test'
    mysql -e "CALL create_table_range($start_idx, $end_idx);" $dbname &
    pids+=($!)  # Store the process ID
}

create_tables() {
    local dbname=${1:-test}
for ((i=0; i<num_threads; i++)); do
    start=$((i * batch_size + 1))
    end=$(( (i + 1) * batch_size ))

    # Handle remainder in the last batch
    if (( i == num_threads - 1 )); then
        end=$num_tables
    fi

    create_tables_parallel "$start" "$end" "$dbname"
done

# Wait only for MySQL processes to finish
for pid in "${pids[@]}"; do
    wait "$pid"
done
}

create_stored_proc
create_tables
drop_stored_proc

# Take backup
vlog "Creating the backup directory: $topdir/backup"
backup_dir="$topdir/backup"
xtrabackup --backup --target-dir=$topdir/full_backup --parallel=$num_threads


insert_records_thread() {
    local start_idx=$1
    local end_idx=$2
    for ((i=start_idx; i<=end_idx; i++)); do
        mysql -e "INSERT INTO t$i (id, name, address, city, country, description) VALUES 
            (NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT),
            (NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT),
            (NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT),
            (NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT),
            (NULL, DEFAULT, DEFAULT, DEFAULT, DEFAULT, DEFAULT); INSERT INTO t$i SELECT NULL, name, address, city, country, description FROM t$i; INSERT INTO t$i SELECT NULL, name, address, city, country, description FROM t$i;INSERT INTO t$i SELECT NULL, name, address, city, country, description FROM t$i; " test
    done &
    pids+=($!)
}

# Clear process ID array
pids=()

# Step 2: Insert records using 16 parallel connections
for ((i=0; i<num_threads; i++)); do
    start=$((i * batch_size + 1))
    end=$(( (i + 1) * batch_size ))
    if (( i == num_threads - 1 )); then
        end=$num_tables
    fi
    insert_records_thread "$start" "$end"
done

# Ensure all inserts complete
for pid in "${pids[@]}"; do
    wait "$pid"
done

stop_server
start_server --innodb_buffer_pool-size=1G --innodb_redo_log_capacity=2G

# Do an incremental parallel backup
xtrabackup --backup --parallel=$num_threads \
    --incremental-basedir=$topdir/full_backup --target-dir=$topdir/inc_backup

stop_server
# Remove datadir
rm -r $mysql_datadir

vlog "Applying log"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/full_backup --parallel=$num_threads
xtrabackup --prepare --apply-log-only --incremental-dir=$topdir/inc_backup --parallel=$num_threads \
    --target-dir=$topdir/full_backup 2> $topdir/inc.log

check_pattern_numbers() {
    local log_file="$1"
    local expected_max="$2"

    if [[ ! -f "$log_file" ]]; then
        echo "Error: File not found!"
        return 1
    fi

    # Calculate the sum of unique numbers before [Note] in lines containing "Applying .* to .*"
    local sum=$(grep -oP '\d+(?= \[Note\] \[MY-011825\] \[Xtrabackup\] Applying .* to .*)' "$log_file" | sort -nu | awk '{sum+=$1} END {print sum}')

    # Expected sum of numbers from 1 to expected_max
    local expected_sum=$((expected_max * (expected_max + 1) / 2))

    if [[ "$sum" -ne "$expected_sum" ]]; then
        echo "Error: Missing numbers in sequence"
        return 1
    fi

    echo "Success: All numbers from 1 to $expected_max are present."
    return 0
}

run_cmd check_pattern_numbers $topdir/inc.log $num_threads

xtrabackup --prepare --target-dir=$topdir/full_backup --parallel=$num_threads

vlog "Restoring MySQL datadir"
mkdir -p $mysql_datadir
xtrabackup --copy-back --target-dir=$topdir/full_backup --parallel=8

start_server
