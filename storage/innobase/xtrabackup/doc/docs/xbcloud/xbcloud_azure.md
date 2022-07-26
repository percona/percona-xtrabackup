# Using the xbcloud binary with Microsoft Azure Cloud Storage

This feature is *technical preview* quality.

Implemented in Percona XtraBackup 8.0.27-19, the **xbcloud** binary adds support for the Microsoft Azure Cloud Storage using the REST API.

## Options

The following are the options, environment variables, and descriptions for uploading a backup to Azure using the REST API. The environment variables are recognized by **xbcloud**, which maps them automatically to the corresponding parameters:

| Option name                  | Environment variables | Description                                                                                                                                                                                                                                                                                                      |
|------------------------------|-----------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| --azure-storage-account=name | AZURE_STORAGE_ACCOUNT | An Azure storage account is a unique namespace to access and store your Azure data objects.                                                                                                                                                                                                                      |
| --azure-container-name=name  | AZURE_CONTAINER_NAME  | A container name is a valid DNS name that conforms to the Azure naming rules                                                                                                                                                                                                                                     |
| --azure-access-key=name      | AZURE_ACCESS_KEY      | A generated key that can be used to authorize access to data in your account using the Shared Key authorization.                                                                                                                                                                                                 |
| --azure-endpoint=name        | AZURE_ENDPOINT        | The endpoint allows clients to securely access data                                                                                                                                                                                                                                                              |
| --azure-tier-class=name      | AZURE_STORAGE_CLASS   | Cloud tier can decrease the local storage required while maintaining the performance. When enabled, this feature has the following categories: <br/><br/>Hot - Frequently accessed or modified data <br/><br/>Cool - Infrequently accessed or modified data <br/><br/>Archive - Rarely accessed or modified data |

Test your Azure applications with the [Azurite open-source emulator](https://docs.microsoft.com/en-us/azure/storage/common/storage-use-azurite?tabs=visual-studio). For testing purposes, the **xbcloud** binary adds the `--azure-development-storage` option that uses the default `access_key` and `storage account` of azurite and `testcontainer` for the container. You can overwrite these options, if needed.

## Usage

All the available options for **xbcloud**, such as parallel, 
max-retries, and others, can be used. For more information, see the
[xbcloud Binary](https://docs.percona.com/percona-xtrabackup/latest/xbcloud/xbcloud.html#xbcloud-binary).

## Examples

An example of an **xbcloud** backup.

```shell
$ xtrabackup --backup --stream=xbstream --target-dir= $TARGET_DIR | 
xbcloud put backup_name --azure-storage-account=pxbtesting --azure-access-key=$AZURE_KEY --azure-container-name=test --storage=azure
```

An example of restoring a backup from **xbcloud**.

```shell
$ xbcloud get backup_name  --azure-storage-account=pxbtesting 
--azure-access-key=$AZURE_KEY --azure-container-name=test --storage=azure --parallel=10 2>download.log | xbstream -x -C restore
```

An example of deleting a backup from **xbcloud**.

```shell
$ xbcloud delete backup_name --azure-storage-account=pxbtesting 
--azure-access-key=$AZURE_KEY --azure-container-name=test --storage=azure
```

An example of using a shortcut restore.

```shell
$ xbcloud get azure://operator-testing/bak22 ...
```
