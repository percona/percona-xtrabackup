--echo #
--echo # Bug#37952274: expr BETWEEN expr and expr fails: Assertion failed
--echo #

--error ER_WRONG_ARGUMENTS
DO 1 BETWEEN TO_BASE64(PERIOD_ADD(1207980960, 2383)) AND 0x43c98093;
