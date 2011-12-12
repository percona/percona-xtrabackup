# suite_config.py
# module containing suite-wide standard variables
# provides single place to tweak how all related tests run

server_requirements = [ [ ("--binlog-do-db=test "
                           "--innodb-file-per-table "
                           "--innodb_file_format='Barracuda' "
                           #"--innodb_log_compressed_pages=0 "
                           #"--innodb_background_checkpoint=0 "
                           "--sync_binlog=100 "
                           "--innodb_flush_log_at_trx_commit=2 "
                           )]
                       ,[ ("--innodb_file_format='Barracuda' "
                           #"--innodb_log_compressed_pages=1 "
                           "--innodb_flush_log_at_trx_commit=2"
                          )]
                      ]
server_requests = {'join_cluster':[(0,1)]}
servers = []
server_manager = None
test_executor = None
randgen_threads = 5
randgen_queries_per_thread = 10000
crashes = 10 
# This only applies to innodbCrash2 test
# as a time delay for when the kill thread
# will stop the master-server
kill_db_after = 20 

