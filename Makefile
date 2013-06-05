MASTER_SITE=http://s3.amazonaws.com/percona.com/downloads/community

.PHONY : dist

# Here we specify what server versions we build against

MYSQL_51_SOURCE=mysql-5.1.59.tar.gz
MYSQL_55_SOURCE=mysql-5.5.17.tar.gz
MYSQL_56_SOURCE=mysql-5.6.10.tar.gz
PS_51_VERSION=5.1.59-13.0
PS_51_SOURCE=Percona-Server-XtraBackup-$(PS_51_VERSION).tar.gz
PS_55_VERSION=5.5.16-22.0
PS_55_SOURCE=Percona-Server-XtraBackup-$(PS_55_VERSION).tar.gz
BZR_REVNO=$(shell bzr revno 2>/dev/null || cat REVNO)
XTRABACKUP_VERSION=$(shell sed -e 's/XTRABACKUP_VERSION=//' < VERSION)

# targets for fetching server source tarballs

SERVER_SOURCE_TARBALLS=$(MYSQL_51_SOURCE) $(MYSQL_55_SOURCE) $(MYSQL_56_SOURCE)
PS_SERVER=$(PS_51_SOURCE) $(PS_55_SOURCE)

# PS server source tarballs are inadequate
# for these older versions, so we have to make them ourselves

.PHONY: ps51source ps55source

ps51source: $(PS_51_SOURCE)


$(PS_51_SOURCE):
	rm -rf percona-server-5.1-xtrabackup
	bzr branch -r tag:Percona-Server-$(PS_51_VERSION) lp:percona-server/5.1 percona-server-5.1-xtrabackup
	cd percona-server-5.1-xtrabackup && bzr export percona-server-5.1-xtrabackup.tar.gz
	mv percona-server-5.1-xtrabackup/percona-server-5.1-xtrabackup.tar.gz .
	rm -rf percona-server-5.1-xtrabackup
	tar xfz percona-server-5.1-xtrabackup.tar.gz
	cd percona-server-5.1-xtrabackup && make mysql-$(shell echo $(PS_51_VERSION) | sed -e 's/-.*//').tar.gz
	rm percona-server-5.1-xtrabackup.tar.gz
	tar cfz $(PS_51_SOURCE) percona-server-5.1-xtrabackup
	rm -rf percona-server-5.1-xtrabackup

ps55source: $(PS_55_SOURCE)

$(PS_55_SOURCE):
	rm -rf percona-server-5.5-xtrabackup
	bzr branch -r tag:Percona-Server-$(PS_55_VERSION) lp:percona-server/5.5 percona-server-5.5-xtrabackup
	cd percona-server-5.5-xtrabackup && bzr export percona-server-5.5-xtrabackup.tar.gz
	mv percona-server-5.5-xtrabackup/percona-server-5.5-xtrabackup.tar.gz .
	rm -rf percona-server-5.5-xtrabackup
	tar xfz percona-server-5.5-xtrabackup.tar.gz
	cd percona-server-5.5-xtrabackup && make mysql-$(shell echo $(PS_55_VERSION) | sed -e 's/-.*//').tar.gz
	rm percona-server-5.5-xtrabackup.tar.gz
	tar cfz $(PS_55_SOURCE) percona-server-5.5-xtrabackup
	rm -rf percona-server-5.5-xtrabackup

# source dist targets

dist: $(SERVER_SOURCE_TARBALLS) $(PS_SERVER)
	bzr export percona-xtrabackup-$(XTRABACKUP_VERSION).tar.gz
	tar xfz percona-xtrabackup-$(XTRABACKUP_VERSION).tar.gz
	cp $(SERVER_SOURCE_TARBALLS) $(PS_SERVER) percona-xtrabackup-$(XTRABACKUP_VERSION)/
	echo $(BZR_REVNO) > percona-xtrabackup-$(XTRABACKUP_VERSION)/REVNO
	rm percona-xtrabackup-$(XTRABACKUP_VERSION).tar.gz
	tar cfz percona-xtrabackup-$(XTRABACKUP_VERSION)-$(BZR_REVNO).tar.gz percona-xtrabackup-$(XTRABACKUP_VERSION)
	rm -rf percona-xtrabackup-$(XTRABACKUP_VERSION)

$(SERVER_SOURCE_TARBALLS):
	wget $(MASTER_SITE)/$@

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
	@echo "innodb55source - source tarball for MySQL 5.1 needed for XB"
	@echo "innodb56source - source tarball for MySQL 5.1 needed for XB"

