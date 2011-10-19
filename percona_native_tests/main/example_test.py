import random
import unittest

server_requirements = [[],[],[]]
servers = []
server_manager = None
test_executor = None

class TestServerUsage(unittest.TestCase):

    def setUp(self):
        self.seq = range(10)

    def test_server_usage(self):
        for server in servers:
            print server.name
            print server.type
            print server.datadir
            print server.master_port


def run_test(output_file):
    suite = unittest.TestLoader().loadTestsFromTestCase(TestSequenceFunctions)
    return unittest.TextTestRunner(stream=output_file, verbosity=2).run(suite)

