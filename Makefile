MASTER_SITE=http://s3.amazonaws.com/percona.com/downloads/community

.PHONY : dist

# Here we specify what server versions we build against

MYSQL_51_SOURCE=mysql-5.1.70.tar.gz
MYSQL_55_SOURCE=mysql-5.5.31.tar.gz
MYSQL_56_SOURCE=mysql-5.6.11.tar.gz
PS_51_SOURCE=Percona-Server-5.1.70-rel14.8.tar.gz
PS_55_SOURCE=Percona-Server-5.5.31-rel30.3.tar.gz
BZR_REVNO=$(shell bzr revno 2>/dev/null || cat REVNO)
XTRABACKUP_VERSION=$(shell sed -e 's/XTRABACKUP_VERSION=//' < VERSION)

# targets for fetching server source tarballs

SERVER_SOURCE_TARBALLS=$(MYSQL_51_SOURCE) $(MYSQL_55_SOURCE) \
  $(MYSQL_56_SOURCE) $(PS_51_SOURCE) $(PS_55_SOURCE)

.PHONY: ps51source ps55source

ps51source: $(PS_51_SOURCE)

ps55source: $(PS_55_SOURCE)

# source dist targets

dist: $(SERVER_SOURCE_TARBALLS)
	bzr export percona-xtrabackup-$(XTRABACKUP_VERSION).tar.gz
	tar xfz percona-xtrabackup-$(XTRABACKUP_VERSION).tar.gz
	test "x$(DUMMY)" != "x" || cp $(SERVER_SOURCE_TARBALLS) percona-xtrabackup-$(XTRABACKUP_VERSION)/
	echo $(BZR_REVNO) > percona-xtrabackup-$(XTRABACKUP_VERSION)/REVNO
	rm percona-xtrabackup-$(XTRABACKUP_VERSION).tar.gz
	tar cfz percona-xtrabackup-$(XTRABACKUP_VERSION)-$(BZR_REVNO).tar.gz percona-xtrabackup-$(XTRABACKUP_VERSION)
	rm -rf percona-xtrabackup-$(XTRABACKUP_VERSION)

$(SERVER_SOURCE_TARBALLS):
	test "x$(DUMMY)" != "x" || wget $(MASTER_SITE)/$@

# fake clean/distclean targets... we explicitly do *NOT* want to clean
# away the tarballs as we actually need to ship them

.PHONY: clean distclean

clean:

distclean:


.PHONY: innodb51source

innodb51source: $(MYSQL_51_SOURCE)

.PHONY: innodb55source

innodb55source: $(MYSQL_55_SOURCE)

.PHONY: innodb56source

innodb56source: $(MYSQL_56_SOURCE)

# HELP

.PHONY: help

help:
	@echo "Build targets for Percona XtraBackup"
	@echo "------------------------------------"
	@echo "Source:"
	@echo
	@echo "dist - source code tarball"
	@echo ""
	@echo "ps51source - source tarball for PS 5.1 for XB"
	@echo "ps55source - source tarball for PS 5.5 for XB"
	@echo "innodb51source - source tarball for MySQL 5.1 needed for XB"
	@echo "innodb55source - source tarball for MySQL 5.5 needed for XB"
	@echo "innodb56source - source tarball for MySQL 5.6 needed for XB"

