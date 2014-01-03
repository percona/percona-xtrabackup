########################################################################
# Bug #1223716: innobackupex --help and --version doesn't work if source
#               destination isn't specifed
########################################################################

innobackupex --help

innobackupex --version

# Check that specifying the directory argument still works

innobackupex --help $topdir

innobackupex --version $topdir
