--let $vault_timeout = 15
if ($vault_conf_timeout)
{
  --let $vault_timeout = $vault_conf_timeout
}
assert($vault_timeout);

--let $vault_url =
if ($vault_conf_address)
{
  --let $vault_url = $vault_conf_address
}
if (!$vault_url)
{
  --let $vault_url = $MTR_VAULT_ADDRESS
}
assert($vault_url != '');

assert($vault_conf_mount_point_uuid != '');
--let $vault_secret_mount_point = `SELECT CONCAT('mtr\/', '$vault_conf_mount_point_uuid')`
if ($vault_conf_mount_point_directory)
{
  --let $vault_secret_mount_point = `SELECT CONCAT('$vault_secret_mount_point', '\/$vault_conf_mount_point_directory')`
}
if ($vault_conf_mount_point_suffix)
{
  --let $vault_secret_mount_point = `SELECT CONCAT('$vault_secret_mount_point', '$vault_conf_mount_point_suffix')`
}

--let $vault_token =
if ($vault_conf_token)
{
  --let $vault_token = $vault_conf_token
}
if (!$vault_token)
{
  --let $vault_token = $MTR_VAULT_PLUGIN_TOKEN
}
assert($vault_token != '');

--let $vault_ca =
if ($vault_conf_ca)
{
  --let $vault_ca = $vault_conf_ca
}
if (!$vault_ca)
{
  --let $vault_ca = $MTR_VAULT_CA
}
if ($skip_vault_conf_ca)
{
  --let $vault_ca = ''
}

--let $config_content = `SELECT CONCAT('\"timeout\": $vault_timeout, ', '\"vault_url\": \"', '$vault_url', '\", \"secret_mount_point\": \"', '$vault_secret_mount_point', '\", \"token\": \"', '$vault_token', '\"')`

if ($vault_ca)
{
  --let $config_content = `SELECT CONCAT('$config_content', ', \"vault_ca\": \"', '$vault_ca', '\"')`
}
if ($vault_conf_mount_point_version)
{
  --let $config_content = `SELECT CONCAT('$config_content', ', \"secret_mount_point_version\": \"', '$vault_conf_mount_point_version', '\"')`
}
