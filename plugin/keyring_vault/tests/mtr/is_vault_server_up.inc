# In order to check whether Vault server is accessible we try to get list
# of server backends. This list should be short.

let KEYRING_CONF_FILE=$KEYRING_CONF_FILE_1;
let SERVER_UUID= query_get_value(SELECT @@SERVER_UUID, @@SERVER_UUID, 1);
if (!$CURL_TIMEOUT)
{
  --let CURL_TIMEOUT=4
}
--perl
  use strict;
  use IO::File;
  my $curl_timeout= $ENV{CURL_TIMEOUT} or die "Need CURL_TIMEOUT";
  my $keyring_conf_file= $ENV{'KEYRING_CONF_FILE'} or die("KEYRING_CONF_FILE not set\n");
  my $server_uuid= $ENV{'SERVER_UUID'} or die("SERVER_UUID not set\n");
  my $token;
  my $vault_url;
  my $secret_mount_point;
  my $vault_ca;
  my $CONF_FILE;
  open(CONF_FILE, "$keyring_conf_file") or die("Could not open configuration file.\n");
  while (my $row = <CONF_FILE>)
  {
    if ($row =~ m/token[ ]*=[ ]*(.*)/)
    {
      $token=$1;
    }
    elsif ($row =~ m/vault_url[ ]*=[ ]*(.*)/)
    {
      $vault_url=$1;
    }
    elsif ($row =~ m/secret_mount_point[ ]*= [ ]*(.*)/)
    {
      $secret_mount_point=$1;
    }
    elsif ($row =~ m/vault_ca[ ]*= [ ]*(.*)/)
    {
      $vault_ca=$1;
    }
  }
  close(CONF_FILE);

  my $vardir= $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";

  if ($token eq "" || $vault_url eq "" || $secret_mount_point eq "")
  {
    die("Could not read vault credentials from configuration file.\n");
  }

  my $vault_ca_cert_opt= "";
  if ($vault_ca)
  {
    $vault_ca_cert_opt= "--cacert $vault_ca";
  }

  system(qq#curl -H "X-Vault-Token: $token" --max-time $curl_timeout $vault_ca_cert_opt $vault_url/v1/sys/mounts > $vardir/tmp/curl_result#);

  my $curl_conn_successful = 1;
  my $curl_response = 0;

  if (!-s "$vardir/tmp/curl_result")
  {
    # result file is empty, thus connection could not be established
    $curl_conn_successful = 0;
  }
  else
  {
    # Vault server has returned errors
    open my $file, '<', "$vardir/tmp/curl_result";
    $curl_response = <$file>;
    if (index($curl_response, "\"errors\":[\"") != -1)
    {
      $curl_conn_successful = 0;
    }
    close $file;
  }

  my $file_name = "$vardir/tmp/mount_list_result.inc";
  my $F = IO::File->new($file_name, 'w') or die "Could not open '$file_name' for writing";
  if (!$curl_conn_successful)
  {
    if ($curl_response)
    {
      print $F "--skip Cannot connect to Hashicorp Vault due to : $curl_response";
    }
    else
    {
      print $F "--skip Seems that Hashicorp Vault testing server is down";
    }
  }
  $F->close();
EOF

--source $MYSQLTEST_VARDIR/tmp/mount_list_result.inc
