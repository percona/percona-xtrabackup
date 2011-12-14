# suite_config.py
# module containing suite-wide standard variables
# provides single place to tweak how all related tests run

server_requirements = [[],[],[]]
server_requests = {'join_cluster':[(0,1), (0,2)]}
servers = []
server_manager = None
test_executor = None

randgen_threads=5
randgen_queries=2000
