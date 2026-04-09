# These tests aim in checking limitations in user and host strings
# in SQL statements.

# Bug#38355483 Incorrect support for spaces in user name

--echo Leading and trailing white spaces must be removed from user and host.
--echo The statements must succeed with warnings.

# left trim
CREATE USER '   user1'@'     host.org';

# right trim
CREATE USER 'user2 '@'host.org		';

# double side trim
CREATE USER '		user3		'@' host.org   ';

# username with spaces inside, no host
CREATE USER ' u s e r 4 ';

# anonymous user
CREATE USER '   '@host.org;

# role
CREATE ROLE ' role1 '@' host.com ', 'role2			';

# ensure the users were created without leading and trailing white spaces
SELECT user, length(user), host, length(host) FROM mysql.user;

# try other user related statements

GRANT ' role2' TO '  user2'@' host.org';
SET DEFAULT ROLE '     role2' TO ' user2 '@host.org;
REVOKE ' role2   ' FROM ' user2 '@'	host.org  ';
ALTER USER 'user3 '@host.org ACCOUNT LOCK;
RENAME USER '	u s e r 4'  TO '  user4 ';
SET PASSWORD FOR user1@'  host.org' = '123';

# drop the users and roles
DROP USER ' user1 '@' host.org ', '   user2'@'  host.org';
DROP USER 'user3	'@'host.org   ', 'user4		';
DROP USER '				'@' host.org ';
DROP ROLE 'role1  '@host.com;
DROP ROLE ' role2 ';
