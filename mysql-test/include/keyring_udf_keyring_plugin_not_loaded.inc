--echo #The keyring plugin was uninstalled, the keyring_udf functions should fail returning NULL

let $is_null= `select isnull(keyring_key_store('Rob_uninstalled','AES','123'))`;
if (!$is_null) {
	die Result should be NULL;
}
let $is_null= `select isnull(keyring_key_fetch('Rob_uninstalled'))`;
if (!$is_null) {
	die Result should be NULL;
}
let $is_null= `select isnull(keyring_key_type_fetch('Rob_uninstalled'))`;
if (!$is_null) {
	die Result should be NULL;
}
let $is_null= `select isnull(keyring_key_length_fetch('Rob_uninstalled'))`;
if (!$is_null) {
	die Result should be NULL;
}
let $is_null= `select isnull(keyring_key_remove('Rob_uninstalled'))`;
if (!$is_null) {
	die Result should be NULL;
}
let $is_null= `select isnull(keyring_key_generate('Rob_uninstalled_generate','DSA',128))`;
if (!$is_null) {
	die Result should be NULL;
}


