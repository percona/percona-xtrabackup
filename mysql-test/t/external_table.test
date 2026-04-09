#
# Test for CREATE EXTERNAL TABLE syntax
#

--let $should_be_skipped = `SELECT @@global.external_table_storage_engine IS NOT NULL`
if ($should_be_skipped)
{
  --skip Test requires external_table_storage_engine to be NULL
}

--echo # Test basic CREATE EXTERNAL TABLE syntax
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SPECIFIED
CREATE EXTERNAL TABLE t1 (id INT);

--echo # Test with explicit ENGINE
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SUPPORTED
CREATE EXTERNAL TABLE t1 (id INT) ENGINE=InnoDB;

--echo # Test with explicit SECONDARY_ENGINE
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SPECIFIED
CREATE EXTERNAL TABLE t1 (id INT) SECONDARY_ENGINE=NULL;

--echo # Test with both engines explicitly set
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SUPPORTED
CREATE EXTERNAL TABLE t1 (id INT) ENGINE=InnoDB SECONDARY_ENGINE=NULL;

--echo # Test with other table options
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SUPPORTED
CREATE EXTERNAL TABLE t1 (id INT)
  COMMENT='External table test'
  AUTO_INCREMENT=100
  ENGINE=InnoDB;

--echo # Test CREATE EXTERNAL TABLE IF NOT EXISTS
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SPECIFIED
CREATE EXTERNAL TABLE IF NOT EXISTS t1 (id INT);

--echo # Test CREATE TEMPORARY EXTERNAL TABLE
--error ER_PARSE_ERROR
CREATE TEMPORARY EXTERNAL TABLE t1 (id INT);

--echo # Test CREATE EXTERNAL TABLE LIKE
CREATE TABLE t_src (id INT, name VARCHAR(50));
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SPECIFIED
CREATE EXTERNAL TABLE t1 LIKE t_src;
DROP TABLE t_src;

--echo # Test CREATE EXTERNAL TABLE with complex column definitions
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SPECIFIED
CREATE EXTERNAL TABLE t1 (
  id INT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(100) NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  status ENUM('active', 'inactive', 'pending') DEFAULT 'pending',
  data JSON,
  INDEX idx_name (name),
  FULLTEXT idx_data (data)
);

--echo # Test variable overrides prior to syntax testing
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SUPPORTED
SET SESSION external_table_storage_engine = 'InnoDB';
SET SESSION external_table_secondary_storage_engine = NULL;
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SPECIFIED
CREATE EXTERNAL TABLE t1 (id INT);

--echo # Reset variables
SET SESSION external_table_storage_engine = DEFAULT;
SET SESSION external_table_secondary_storage_engine = DEFAULT;


# Test for external table system variables

--echo # Test setting and getting session system variables
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SUPPORTED
SET SESSION external_table_storage_engine = 'MyISAM';
SET SESSION external_table_secondary_storage_engine = 'MEMORY';
SELECT @@session.external_table_storage_engine, @@session.external_table_secondary_storage_engine;

--echo # Test error with unknown engine
--error  ER_UNKNOWN_STORAGE_ENGINE
SET SESSION external_table_storage_engine = 'NONEXISTENT_ENGINE';

--echo # Test setting to NULL
SET SESSION external_table_secondary_storage_engine = NULL;
SELECT @@session.external_table_secondary_storage_engine;

--echo # Reset session variables to defaults
SET SESSION external_table_storage_engine = DEFAULT;
SET SESSION external_table_secondary_storage_engine = DEFAULT;
SELECT @@session.external_table_storage_engine, @@session.external_table_secondary_storage_engine;

--echo # Test setting and getting global system variables
--error ER_EXTERNAL_TABLE_ENGINE_NOT_SUPPORTED
SET GLOBAL external_table_storage_engine = 'MyISAM';
SET GLOBAL external_table_secondary_storage_engine = 'MEMORY';
SELECT @@global.external_table_storage_engine, @@global.external_table_secondary_storage_engine;

--echo # Test global vs session variables
SELECT @@global.external_table_storage_engine, @@session.external_table_storage_engine, 
       @@global.external_table_secondary_storage_engine, @@session.external_table_secondary_storage_engine;

--echo # Reset global variables to defaults
SET GLOBAL external_table_storage_engine = DEFAULT;
SET GLOBAL external_table_secondary_storage_engine = DEFAULT;

--echo # Test command line initialization with valid values
--let $restart_parameters = restart: --external-table-storage-engine= --external-table-secondary-storage-engine=
--source include/restart_mysqld.inc

--echo # Check that variables were set correctly from command line
SELECT @@global.external_table_storage_engine, @@global.external_table_secondary_storage_engine;

--echo # Final cleanup
SET GLOBAL external_table_storage_engine = DEFAULT;
SET GLOBAL external_table_secondary_storage_engine = DEFAULT;
