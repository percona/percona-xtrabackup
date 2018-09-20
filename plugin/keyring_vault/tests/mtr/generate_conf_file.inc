let KEYRING_CONF_FILE_TO_GENERATE=$KEYRING_CONF_FILE_TO_GENERATE;
let KEYRING_CONF_TEMPLATE_FILE=$KEYRING_CONF_TEMPLATE_FILE;
let MYSQL_TEST_DIR=$MYSQL_TEST_DIR;
let SERVER_UUID= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1);

--perl
  use strict;
  my $mysql_test_dir= $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
  my $server_uuid= $ENV{SERVER_UUID} or die "Server uuid not set";
  my $keyring_conf_template_file= $ENV{KEYRING_CONF_TEMPLATE_FILE} or die "Need KEYRING_CONF_TEMPLATE_FILE";
  my $keyring_conf_file_to_generate= $ENV{'KEYRING_CONF_FILE_TO_GENERATE'} or die("KEYRING_CONF_FILE_TO_GENERATE not set\n");
  open CONF_FILE, ">", "$keyring_conf_file_to_generate" or die "Could not open configuration file: ${keyring_conf_file_to_generate}.\n";
  open CONF_TEMPLATE_FILE, "<", "$keyring_conf_template_file" or die "Could not open configuration template file: ${keyring_conf_template_file}.\n";
  while (my $row = <CONF_TEMPLATE_FILE>)
  {
    $row =~ s/MYSQL_TEST_DIR/$mysql_test_dir/g;
    $row =~ s/SECRET_MOUNT_POINT_TAG/$server_uuid/g;
    print CONF_FILE $row;
  }
  close(CONF_FILE);
  close(CONF_TEMPLATE_FILE);
EOF
