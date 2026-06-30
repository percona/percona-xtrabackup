############################################################################
# Test --check-tables on prepare: positive multi-combination scenario,
# real IBD corruption (broken sibling link), and page checksum corruption.
#
# Scenario 1 covers: encrypted, compressed, plain, general tablespace,
#   FTS (fulltext), INSTANT ALTER, functional index, virtual column index,
#   GIS/spatial index, and HASH-partitioned tables.
# Scenario 6: Invalid --check-tables combinations (--copy-back, --backup).
# Scenario 7: --prepare --export --check-tables combined.
############################################################################

. inc/keyring_file.sh
. inc/keyring_common.sh

configure_server_with_component

mysql test <<'EOF'
CREATE TABLE t_enc (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) ENCRYPTION='y';
CREATE TABLE t_comp (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=4;
CREATE TABLE t_plain (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100));
CREATE TABLESPACE ts_general ADD DATAFILE 'ts_general.ibd' ENGINE=InnoDB;
CREATE TABLE t_gen1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) TABLESPACE ts_general;
CREATE TABLE t_gen2 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) TABLESPACE ts_general;
CREATE TABLE t_gen3 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) TABLESPACE ts_general;
CREATE TABLE t_gen4 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) TABLESPACE ts_general;
CREATE TABLE t_gen5 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) TABLESPACE ts_general;
CREATE TABLE t_fts (a INT PRIMARY KEY AUTO_INCREMENT, b TEXT, FULLTEXT INDEX ft_idx(b)) ENGINE=InnoDB;
CREATE TABLE t_instant (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100));
CREATE TABLE t_funcidx (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100), INDEX idx_func((UPPER(b))));
CREATE TABLE t_virtual (a INT PRIMARY KEY AUTO_INCREMENT, b INT, c INT AS (b * 2) VIRTUAL, INDEX idx_virtual(c));
CREATE TABLE t_gis (a INT PRIMARY KEY AUTO_INCREMENT, g GEOMETRY NOT NULL SRID 0, SPATIAL INDEX idx_gis(g));
CREATE TABLE t_part (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100)) PARTITION BY HASH(a) PARTITIONS 3;
CREATE TABLE t_part_range (a INT NOT NULL, b VARCHAR(100), PRIMARY KEY (a)) PARTITION BY RANGE(a) (PARTITION p0 VALUES LESS THAN (101), PARTITION p1 VALUES LESS THAN (201), PARTITION pmax VALUES LESS THAN MAXVALUE);
CREATE TABLE t_json_mvi (a INT PRIMARY KEY AUTO_INCREMENT, doc JSON, tags JSON, KEY idx_tags ((CAST(tags->'$[*]' AS UNSIGNED ARRAY))));
CREATE TABLE t_fk_parent (a INT PRIMARY KEY AUTO_INCREMENT, code VARCHAR(10) NOT NULL, UNIQUE KEY uk_code (code));
CREATE TABLE t_fk_child (a INT PRIMARY KEY AUTO_INCREMENT, parent_id INT, parent_code VARCHAR(10), KEY idx_pid (parent_id), KEY idx_code (parent_code), CONSTRAINT fk_pid FOREIGN KEY (parent_id) REFERENCES t_fk_parent (a) ON DELETE CASCADE, CONSTRAINT fk_code FOREIGN KEY (parent_code) REFERENCES t_fk_parent (code) ON DELETE SET NULL);
CREATE TABLE t_invisible (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(100), c INT INVISIBLE, d INT AS (c * 2) VIRTUAL INVISIBLE, KEY idx_c (c));
EOF

for i in $(seq 1 100); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t_enc (b) VALUES ('enc_init_${i}');
     INSERT INTO t_comp (b) VALUES ('comp_init_${i}');
     INSERT INTO t_plain (b) VALUES ('plain_init_${i}');
     INSERT INTO t_gen1 (b) VALUES ('gen1_init_${i}');
     INSERT INTO t_gen2 (b) VALUES ('gen2_init_${i}');
     INSERT INTO t_gen3 (b) VALUES ('gen3_init_${i}');
     INSERT INTO t_gen4 (b) VALUES ('gen4_init_${i}');
     INSERT INTO t_gen5 (b) VALUES ('gen5_init_${i}');
     INSERT INTO t_fts (b) VALUES ('fts_init_${i}');
     INSERT INTO t_instant (b) VALUES ('inst_init_${i}');
     INSERT INTO t_funcidx (b) VALUES ('func_init_${i}');
     INSERT INTO t_virtual (b) VALUES (${i});
     INSERT INTO t_gis (g) VALUES (ST_GeomFromText('POINT(${i} ${i})'));
     INSERT INTO t_part (b) VALUES ('part_init_${i}');
     INSERT INTO t_part_range (a, b) VALUES (${i}, 'range_init_${i}');
     INSERT INTO t_json_mvi (doc, tags) VALUES ('{\"name\": \"u${i}\"}', '[${i}, $((i+1))]');
     INSERT INTO t_fk_parent (code) VALUES ('C${i}');
     INSERT INTO t_fk_child (parent_id, parent_code) VALUES (${i}, 'C${i}');
     INSERT INTO t_invisible (b, c) VALUES ('inv_init_${i}', ${i});"
done

vlog "INSTANT ALTER: add column c to t_instant after initial data"
mysql test <<EOF
ALTER TABLE t_instant ADD COLUMN c INT DEFAULT 42, ALGORITHM=INSTANT;
ALTER TABLE t_instant ADD COLUMN d VARCHAR(50) DEFAULT 'added', ALGORITHM=INSTANT;
ALTER TABLE t_instant DROP COLUMN d, ALGORITHM=INSTANT;
ALTER TABLE t_instant ADD COLUMN e BIGINT, ALGORITHM=INSTANT;
EOF

vlog "Full backup"
xtrabackup --backup --target-dir=$topdir/full \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup_full.log >&2)

for i in $(seq 101 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t_enc (b) VALUES ('enc_inc1_${i}');
     INSERT INTO t_comp (b) VALUES ('comp_inc1_${i}');
     INSERT INTO t_plain (b) VALUES ('plain_inc1_${i}');
     INSERT INTO t_gen1 (b) VALUES ('gen1_inc1_${i}');
     INSERT INTO t_gen2 (b) VALUES ('gen2_inc1_${i}');
     INSERT INTO t_gen3 (b) VALUES ('gen3_inc1_${i}');
     INSERT INTO t_gen4 (b) VALUES ('gen4_inc1_${i}');
     INSERT INTO t_gen5 (b) VALUES ('gen5_inc1_${i}');
     INSERT INTO t_fts (b) VALUES ('fts_inc1_${i}');
     INSERT INTO t_instant (b, c, e) VALUES ('inst_inc1_${i}', ${i}, ${i}*10);
     INSERT INTO t_funcidx (b) VALUES ('func_inc1_${i}');
     INSERT INTO t_virtual (b) VALUES (${i});
     INSERT INTO t_gis (g) VALUES (ST_GeomFromText('POINT(${i} ${i})'));
     INSERT INTO t_part (b) VALUES ('part_inc1_${i}');
     INSERT INTO t_part_range (a, b) VALUES (${i}, 'range_inc1_${i}');
     INSERT INTO t_json_mvi (doc, tags) VALUES ('{\"name\": \"u${i}\"}', '[${i}, $((i+1))]');
     INSERT INTO t_fk_parent (code) VALUES ('D${i}');
     INSERT INTO t_fk_child (parent_id, parent_code) VALUES (${i}, 'D${i}');
     INSERT INTO t_invisible (b, c) VALUES ('inv_inc1_${i}', ${i});"
done

vlog "Incremental backup 1"
xtrabackup --backup --incremental-basedir=$topdir/full \
           --target-dir=$topdir/inc1 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup_inc1.log >&2)

for i in $(seq 201 300); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t_enc (b) VALUES ('enc_inc2_${i}');
     INSERT INTO t_comp (b) VALUES ('comp_inc2_${i}');
     INSERT INTO t_plain (b) VALUES ('plain_inc2_${i}');
     INSERT INTO t_gen1 (b) VALUES ('gen1_inc2_${i}');
     INSERT INTO t_gen2 (b) VALUES ('gen2_inc2_${i}');
     INSERT INTO t_gen3 (b) VALUES ('gen3_inc2_${i}');
     INSERT INTO t_gen4 (b) VALUES ('gen4_inc2_${i}');
     INSERT INTO t_gen5 (b) VALUES ('gen5_inc2_${i}');
     INSERT INTO t_fts (b) VALUES ('fts_inc2_${i}');
     INSERT INTO t_instant (b, c, e) VALUES ('inst_inc2_${i}', ${i}, ${i}*10);
     INSERT INTO t_funcidx (b) VALUES ('func_inc2_${i}');
     INSERT INTO t_virtual (b) VALUES (${i});
     INSERT INTO t_gis (g) VALUES (ST_GeomFromText('POINT(${i} ${i})'));
     INSERT INTO t_part (b) VALUES ('part_inc2_${i}');
     INSERT INTO t_part_range (a, b) VALUES (${i}, 'range_inc2_${i}');
     INSERT INTO t_json_mvi (doc, tags) VALUES ('{\"name\": \"u${i}\"}', '[${i}, $((i+1))]');
     INSERT INTO t_fk_parent (code) VALUES ('E${i}');
     INSERT INTO t_fk_child (parent_id, parent_code) VALUES (${i}, 'E${i}');
     INSERT INTO t_invisible (b, c) VALUES ('inv_inc2_${i}', ${i});"
done

vlog "Incremental backup 2"
xtrabackup --backup --incremental-basedir=$topdir/inc1 \
           --target-dir=$topdir/inc2 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup_inc2.log >&2)

record_db_state test

vlog "Prepare full with --apply-log-only --check-tables (should run check)"
xtrabackup --prepare --apply-log-only --check-tables --target-dir=$topdir/full \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2>&1 | tee $topdir/prepare_full.log
grep log-applied $topdir/full/xtrabackup_checkpoints
grep -q "Starting table checks" $topdir/prepare_full.log || \
  die "check-tables should run during --apply-log-only prepare"
grep -q "All table checks passed" $topdir/prepare_full.log || \
  die "Table checks did not pass during --apply-log-only prepare"

vlog "Prepare inc1 with --apply-log-only --check-tables (should run check)"
xtrabackup --prepare --apply-log-only --check-tables \
           --incremental-dir=$topdir/inc1 --target-dir=$topdir/full \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2>&1 | tee $topdir/prepare_inc1.log
grep log-applied $topdir/full/xtrabackup_checkpoints
grep -q "Starting table checks" $topdir/prepare_inc1.log || \
  die "check-tables should run during incremental --apply-log-only prepare"
grep -q "All table checks passed" $topdir/prepare_inc1.log || \
  die "Table checks did not pass during incremental --apply-log-only prepare"

vlog "Prepare inc2 with --check-tables (final prepare, should run check)"
xtrabackup --prepare --check-tables \
           --incremental-dir=$topdir/inc2 --target-dir=$topdir/full \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2>&1 | tee $topdir/prepare_inc2.log
grep full-prepared $topdir/full/xtrabackup_checkpoints
grep -q "Starting table checks" $topdir/prepare_inc2.log || \
  die "check-tables did not start on final prepare"
grep -q "Checking: test/t_enc" $topdir/prepare_inc2.log || \
  die "Encrypted table not checked"
grep -q "Checking: test/t_comp" $topdir/prepare_inc2.log || \
  die "Compressed table not checked"
grep -q "Checking: test/t_plain" $topdir/prepare_inc2.log || \
  die "Plain table not checked"
grep -q "Checking: test/t_gen1" $topdir/prepare_inc2.log || \
  die "General tablespace table t_gen1 not checked"
grep -q "Checking: test/t_gen2" $topdir/prepare_inc2.log || \
  die "General tablespace table t_gen2 not checked"
grep -q "Checking: test/t_gen3" $topdir/prepare_inc2.log || \
  die "General tablespace table t_gen3 not checked"
grep -q "Checking: test/t_gen4" $topdir/prepare_inc2.log || \
  die "General tablespace table t_gen4 not checked"
grep -q "Checking: test/t_gen5" $topdir/prepare_inc2.log || \
  die "General tablespace table t_gen5 not checked"
grep -q "Checking: test/t_fts" $topdir/prepare_inc2.log || \
  die "FTS table not checked"
grep -q "Checking: test/t_instant" $topdir/prepare_inc2.log || \
  die "INSTANT ALTER table not checked"
grep -q "Checking: test/t_funcidx" $topdir/prepare_inc2.log || \
  die "Functional index table not checked"
grep -q "Checking: test/t_virtual" $topdir/prepare_inc2.log || \
  die "Virtual column index table not checked"
grep -q "Checking: test/t_gis" $topdir/prepare_inc2.log || \
  die "GIS/spatial table not checked"
grep -q "Checking: test/t_part" $topdir/prepare_inc2.log || \
  die "Partitioned table not checked"
grep -q "Checking: test/t_part_range" $topdir/prepare_inc2.log || \
  die "RANGE partitioned table not checked"
grep -q "Checking: test/t_json_mvi" $topdir/prepare_inc2.log || \
  die "JSON multi-valued index table not checked"
grep -q "Checking: test/t_fk_parent" $topdir/prepare_inc2.log || \
  die "FK parent table not checked"
grep -q "Checking: test/t_fk_child" $topdir/prepare_inc2.log || \
  die "FK child table not checked"
grep -q "Checking: test/t_invisible" $topdir/prepare_inc2.log || \
  die "Invisible columns table not checked"
grep -q "All table checks passed" $topdir/prepare_inc2.log || \
  die "Table checks did not pass"

vlog "Restore and verify"
stop_server
rm -rf $mysql_datadir/*
xtrabackup --copy-back --target-dir=$topdir/full
configure_keyring_file_component
start_server
verify_db_state test

#
# Scenario 2: Real IBD corruption (broken FIL_PAGE_NEXT sibling link)
#
vlog "=== Scenario 2: Real IBD corruption ==="

vlog "Create a table with enough rows for multiple leaf pages"
mysql test <<EOF
CREATE TABLE t1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF

for i in $(seq 1 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t1 (b) VALUES (REPEAT('x', 200));"
done

xtrabackup --backup --target-dir=$topdir/backup2 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup2.log >&2)

vlog "Prepare with --apply-log-only first"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup2 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/prepare_s2_alog.log >&2)

vlog "Corrupt PAGE_INDEX_ID on leaf page 6 of test/t1.ibd"
# PAGE_INDEX_ID sits at PAGE_HEADER(38) + 28 = 66 bytes from page start
mach_write_8 "$topdir/backup2/test/t1.ibd" 6 66 0xFFFFFFFFFFFFFFFF
$MYSQL_BASEDIR/bin/innochecksum -w crc32 --no-check $topdir/backup2/test/t1.ibd

vlog "Prepare with --check-tables should fail"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
  --target-dir=$topdir/backup2 2>&1 | tee $topdir/prepare_corrupt.log

grep -q "Starting table checks" $topdir/prepare_corrupt.log || \
  die "check-tables did not start"
grep -q "is corrupted" $topdir/prepare_corrupt.log || \
  die "Corruption not detected"
grep -q "Table check failed" $topdir/prepare_corrupt.log || \
  die "Table check failed message not found"

#
# Scenario 3: Page checksum corruption (no checksum fix)
#
vlog "=== Scenario 3: Page checksum corruption ==="

mysql test <<EOF
CREATE TABLE t2 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF

for i in $(seq 1 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t2 (b) VALUES (REPEAT('y', 200));"
done

xtrabackup --backup --target-dir=$topdir/backup3 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup3.log >&2)

vlog "Prepare with --apply-log-only first"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup3 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/prepare_s3_alog.log >&2)

vlog "Corrupt a data byte in page 4 WITHOUT fixing checksum"
mach_write_n "$topdir/backup3/test/t2.ibd" 4 100 0xDE 1

vlog "Prepare with --check-tables should fail (checksum mismatch)"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
  --target-dir=$topdir/backup3 2>&1 | tee $topdir/prepare_checksum.log

vlog "Scenario 3 passed: xtrabackup exited with non-zero status"

#
# Scenario 4: All-zero page (simulates unflushed page from redo)
#
vlog "=== Scenario 4: All-zero page corruption ==="

mysql test <<EOF
CREATE TABLE t3 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF

for i in $(seq 1 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t3 (b) VALUES (REPEAT('z', 200));"
done

xtrabackup --backup --target-dir=$topdir/backup4 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup4.log >&2)

vlog "Prepare with --apply-log-only first"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup4 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/prepare_s4_alog.log >&2)

vlog "Zero out leaf page 5 to simulate unflushed page"
page_size=16384
page_no=5
dd if=/dev/zero of=$topdir/backup4/test/t3.ibd \
  bs=$page_size seek=$page_no count=1 conv=notrunc

vlog "Prepare with --check-tables should fail (all-zero page detected)"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
  --target-dir=$topdir/backup4 2>&1 | tee $topdir/prepare_zero.log

grep -q "Starting table checks" $topdir/prepare_zero.log || \
  die "check-tables did not start"
grep -q "is corrupted" $topdir/prepare_zero.log || \
  die "All-zero page corruption not detected at index level"
grep -q "Table check failed" $topdir/prepare_zero.log || \
  die "Table check failed message not found for all-zero page"

vlog "Scenario 4 passed: all-zero page corruption detected gracefully"

#
# Scenario 5: Multiple corrupted tables -- verify ALL are reported
#
vlog "=== Scenario 5: Multiple corrupted tables ==="

mysql test <<EOF
CREATE TABLE t_multi1 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
CREATE TABLE t_multi2 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
CREATE TABLE t_multi3 (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF

for i in $(seq 1 200); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t_multi1 (b) VALUES (REPEAT('m', 200));
     INSERT INTO t_multi2 (b) VALUES (REPEAT('n', 200));
     INSERT INTO t_multi3 (b) VALUES (REPEAT('o', 200));"
done

xtrabackup --backup --target-dir=$topdir/backup5 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup5.log >&2)

vlog "Prepare with --apply-log-only first"
xtrabackup --prepare --apply-log-only --target-dir=$topdir/backup5 \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/prepare_s5_alog.log >&2)

vlog "Corrupt PAGE_INDEX_ID on all three tables"
for tbl in t_multi1 t_multi2 t_multi3; do
  mach_write_8 "$topdir/backup5/test/${tbl}.ibd" 5 66 0xFFFFFFFFFFFFFFFF
  $MYSQL_BASEDIR/bin/innochecksum -w crc32 --no-check $topdir/backup5/test/${tbl}.ibd
done

vlog "Prepare with --check-tables should fail and report ALL corrupted tables"
run_cmd_expect_failure $XB_BIN $XB_ARGS --prepare --check-tables \
  --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
  --target-dir=$topdir/backup5 2>&1 | tee $topdir/prepare_multi.log

grep -q "test/t_multi1" $topdir/prepare_multi.log || \
  die "Corruption in t_multi1 not reported"
grep -q "test/t_multi2" $topdir/prepare_multi.log || \
  die "Corruption in t_multi2 not reported"
grep -q "test/t_multi3" $topdir/prepare_multi.log || \
  die "Corruption in t_multi3 not reported"

corrupt_count=$(grep -c "is corrupted" $topdir/prepare_multi.log)
if [ "$corrupt_count" -lt 3 ]; then
  die "Expected at least 3 'is corrupted' messages, got $corrupt_count"
fi

grep -q "Table check failed" $topdir/prepare_multi.log || \
  die "Table check failed message not found"

vlog "Scenario 5 passed: all corrupted tables reported"

#
# Scenario 6: Invalid --check-tables combinations
#
vlog "=== Scenario 6: Invalid --check-tables combinations ==="

vlog "Test --copy-back --check-tables (should fail)"
run_cmd_expect_failure $XB_BIN $XB_ARGS --copy-back --check-tables \
  --target-dir=$topdir/full 2>&1 | tee $topdir/invalid_copyback.log
grep -q "check-tables is only valid with --prepare" $topdir/invalid_copyback.log || \
  die "--copy-back --check-tables did not produce expected error"

vlog "Test --backup --check-tables (should fail)"
run_cmd_expect_failure $XB_BIN $XB_ARGS --backup --check-tables \
  --target-dir=$topdir/backup_invalid 2>&1 | tee $topdir/invalid_backup.log
grep -q "check-tables is only valid with --prepare" $topdir/invalid_backup.log || \
  die "--backup --check-tables did not produce expected error"

vlog "Scenario 6 passed: invalid combinations rejected"

#
# Scenario 7: --prepare --export --check-tables (combined)
#
vlog "=== Scenario 7: --prepare --export --check-tables ==="

mysql test <<EOF
CREATE TABLE t_export (a INT PRIMARY KEY AUTO_INCREMENT, b VARCHAR(200));
EOF

for i in $(seq 1 100); do
  run_cmd $MYSQL $MYSQL_ARGS test -e \
    "INSERT INTO t_export (b) VALUES ('export_${i}');"
done

xtrabackup --backup --target-dir=$topdir/backup_export \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2> >(tee $topdir/backup_export.log >&2)

vlog "Prepare with --export --check-tables"
xtrabackup --prepare --export --check-tables \
           --target-dir=$topdir/backup_export \
           --xtrabackup-plugin-dir=${plugin_dir} ${keyring_args} \
           2>&1 | tee $topdir/prepare_export.log

grep -q "export option is specified" $topdir/prepare_export.log || \
  die "Export did not run"
grep -q "Starting table checks" $topdir/prepare_export.log || \
  die "check-tables did not run with --export"
grep -q "Checking: test/t_export" $topdir/prepare_export.log || \
  die "Export table not checked"
grep -q "All table checks passed" $topdir/prepare_export.log || \
  die "Table checks did not pass with --export"

test -f $topdir/backup_export/test/t_export.cfg || \
  die ".cfg export file not created"

vlog "Scenario 7 passed: --prepare --export --check-tables works correctly"
