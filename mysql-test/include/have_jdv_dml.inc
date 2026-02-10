--disable_query_log

SET @save_log_bin = @@SQL_LOG_BIN;
SET session SQL_LOG_BIN = 0;

--let $_jdv_dml_not_available_error= convert_error(ER_JDV_FEATURE_EDITION_LIMIT)
CREATE TABLE __jdv_tab(f1 INT PRIMARY KEY);
CREATE OR REPLACE JSON DUALITY VIEW __jdv_dv AS SELECT JSON_DUALITY_OBJECT(WITH (INSERT, UPDATE, DELETE) "_id" : f1) FROM __jdv_tab;

--error 0, $_jdv_dml_not_available_error
INSERT INTO __jdv_dv VALUES('{ "_id" : 1 }');
--let $_ins_error= query_get_value(SHOW ERRORS, Code, 1)

DROP VIEW __jdv_dv;
DROP TABLE __jdv_tab;

SET session SQL_LOG_BIN = @save_log_bin;

--enable_query_log

if (!$skip_if_jdv_dml_supported) {
  --let $skip_if_jdv_dml_supported= 0
}

if ($skip_if_jdv_dml_supported == 1){
  if ($_ins_error != $_jdv_dml_not_available_error){
    --skip DML on JSON duality view is supported
  }
}

if ($skip_if_jdv_dml_supported == 0){
  if ($_ins_error == $_jdv_dml_not_available_error){
    --skip DML on JSON duality view is not supported
  }
}