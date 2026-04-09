--echo # Bug#36892499: use database doesn't work

--echo # Connection 1
connect(con1, localhost, root,,);
CREATE DATABASE testDB;
use testDB;
CREATE TABLE t1 (c1 INT);
INSERT INTO t1 (c1) VALUES(1);
INSERT INTO t1 (c1) VALUES(2);
INSERT INTO t1 (c1) VALUES(3);
begin;
select * from t1;

--echo # Connection 2
connect(con2, localhost, root,,);
use testDB;
--send drop table t1;

--echo # Connection 3
connect(con3, localhost, root,,);
use testDB;

--connection con1
commit;
--connection con2
reap;

#Cleanup
drop database testDB;
disconnect con1;
disconnect con2;
disconnect con3;
