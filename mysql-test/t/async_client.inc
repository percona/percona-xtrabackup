CREATE USER foo@localhost IDENTIFIED BY 'pwd';
enable_async_client;

--echo #
--echo # Nonblocking client
--echo # Send/Recieve a large packet with following options :-
--echo # Compress option : $COMPRESS_OPTION
--echo # Compress algorithm : $COMPRESS_ALGORITHM
--echo # Packet size : $SIZE
--echo #
if($COMPRESS_OPTION)
{
 connect(foo_con,localhost,foo,pwd,,,,SSL $COMPRESS_OPTION,,$COMPRESS_ALGORITHM);
}
if(!$COMPRESS_OPTION)
{
connect(foo_con,localhost,foo,pwd,,,,SSL,,);
}

--let $x_str = `SELECT REPEAT('X', $SIZE)`
--let $SIZE =  `SELECT ($SIZE)`

--echo ############ Verify Send and Recieve $SIZE bytes packet #################

--let $str = `SELECT "$x_str"`
--let $x_str = 0

--echo ############ Verify the packet contents #################################
--let $digest_client = `SELECT SHA2("$str",256)`
--let $digest_server = `SELECT SHA2(REPEAT('X', $SIZE),256)`
if($digest_client != $digest_server)
{
 die "Digests do not match";
}

connection default;
disconnect foo_con;
disable_async_client;

#cleanup
DROP USER foo@localhost;
