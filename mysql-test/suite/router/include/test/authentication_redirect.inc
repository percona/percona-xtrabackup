if ($test_idx_val)
{
  --inc $test_idx_val
}

if (!$test_idx_val)
{
  --let $test_idx_val=1
}

if ($test_idx_val == 1)
{
  --let $test_idx=I
}

if ($test_idx_val == 2)
{
  --let $test_idx=II
}

--echo
--echo #
--echo # $test_idx.1

exec $MRS_CLIENT_ARGS
  -a BASIC
  --path /$test_service/authentication/login
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev%0Aservice.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev%0Ahttps://www.service.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev%09service.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev%09https://www.service.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev%0Bservice.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev%0Bhttps://www.service.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://a%20.pl//www.something.ev
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=a%00.pl//www.evil.ev
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://a%00.pl//www.evil.ev
  -u mrsuser -p S3kre7;


--echo
--echo #
--echo # $test_idx.2
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev//https://www.service.local/something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev/x//something/1
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev/x//
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.evil.ev//something/1
  -u mrsuser -p S3kre7;

--echo
--echo #
--echo # $test_idx.3
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.service.local/something/1@https://www.evil.ev
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.service.local@https://www.evil.ev/something
  -u mrsuser -p S3kre7;


--echo
--echo #
--echo # $test_idx.4
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=//www.evil.ev
  -u mrsuser -p S3kre7;


--echo
--echo #
--echo # $test_idx.5
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=%20//www.evil.ev
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=a%20.pl//www.evil.ev
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=a.pl://www.evil.ev
  -u mrsuser -p S3kre7;

exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=mysql://www.evil.ev
  -u mrsuser -p S3kre7;


--echo
--echo #
--echo # $test_idx.6
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=www.host_without_scheme.com
  -u mrsuser -p S3kre7;


--echo
--echo #
--echo # $test_idx.7
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=https://www.host_without_scheme.com/path%0Asomething/1
  -u mrsuser -p S3kre7;


--echo
--echo #
--echo # $test_idx.8
exec $MRS_CLIENT_ARGS
  -a BASIC
  --expected-status Unauthorized
  --path /$test_service/authentication/login?onCompletionRedirect=../../admin
  -u mrsuser -p S3kre7;
