# This file can be outside of mysql-test-run to drop the test and
# result tables for the CRUND benchmark environment described under
# storage/ndb/test/crund
#
# mysql -u root < crund_create.inc
#

USE crunddb;

DROP VIEW results_rtime_nrows;
DROP VIEW results_rtime_nrows4;
DROP VIEW results_rtime_nrows3;
DROP VIEW results_rtime_nrows2;
DROP VIEW results_rtime_xmode;
DROP VIEW results_rtime_bulk;
DROP VIEW results_rtime_each;
DROP VIEW results_rtime_indy;
DROP TABLE results;
DROP TABLE s;
DROP TABLE b;
DROP TABLE a;

DROP DATABASE crunddb;
