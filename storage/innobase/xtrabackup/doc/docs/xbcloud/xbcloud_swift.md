# Using the xbcloud Binary with Swift

## Creating a full backup with Swift

The following example shows how to make a full backup and upload it to Swift.

```
$ xtrabackup --backup --stream=xbstream --extra-lsndir=/tmp --target-dir=/tmp | \
xbcloud put --storage=swift \
--swift-container=test \
--swift-user=test:tester \
--swift-auth-url=http://192.168.8.80:8080/ \
--swift-key=testing \
--parallel=10 \
full_backup
```

The following OpenStack environment variables are also recognized and mapped automatically to the corresponding **swift** parameters (`--storage=swift`):

> 
> * OS_AUTH_URL


> * OS_TENANT_NAME


> * OS_TENANT_ID


> * OS_USERNAME


> * OS_PASSWORD


> * OS_USER_DOMAIN


> * OS_USER_DOMAIN_ID


> * OS_PROJECT_DOMAIN


> * OS_PROJECT_DOMAIN_ID


> * OS_REGION_NAME


> * OS_STORAGE_URL


> * OS_CACERT

## Restoring with Swift

```shell
$ xbcloud get [options] <name> [<list-of-files>] | xbstream -x
```

The following example shows how to fetch and restore the backup from Swift:

```shell
$ xbcloud get --storage=swift \
--swift-container=test \
--swift-user=test:tester \
--swift-auth-url=http://192.168.8.80:8080/ \
--swift-key=testing \
full_backup | xbstream -xv -C /tmp/downloaded_full

$ xbcloud delete --storage=swift --swift-user=xtrabackup \
--swift-password=xtrabackup123! --swift-auth-version=3 \
--swift-auth-url=http://openstack.ci.percona.com:5000/ \
--swift-container=mybackup1 --swift-domain=Default
```

## Command-line options

*xbcloud* has the following command line options:


### --storage(=[swift|s3|google])
Cloud storage option. *xbcloud* supports Swift, MinIO, and AWS S3.
The default value is `swift`.


### --swift-auth-url()
URL of Swift cluster.


### --swift-storage-url()
xbcloud will try to get object-store URL for given region (if any specified)
from the keystone response. One can override that URL by passing
–swift-storage-url=URL argument.


### --swift-user()
Swift username (X-Auth-User, specific to Swift)


### --swift-key()
Swift key/password (X-Auth-Key, specific to Swift)


### --swift-container()
Container to backup into (specific to Swift)


### --parallel(=N)
Maximum number of concurrent upload/download requests. Default is `1`.


### --cacert()
Path to the file with CA certificates


### --insecure()
Do not verify servers certificate

### Swift authentication options

Swift specification describes several [authentication options](http://docs.openstack.org/developer/swift/overview_auth.html). *xbcloud* can
authenticate against keystone with API version 2 and 3.


### --swift-auth-version()
Specifies the swift authentication version. Possible values are: `1.0` -
TempAuth, `2.0` - Keystone v2.0, and `3` - Keystone v3. Default value is
`1.0`.

For v2 additional options are:


### --swift-tenant()
Swift tenant name.


### --swift-tenant-id()
Swift tenant ID.


### --swift-region()
Swift endpoint region.


### --swift-password()
Swift password for the user.

For v3 additional options are:


### --swift-user-id()
Swift user ID.


### --swift-project()
Swift project name.


### --swift-project-id()
Swift project ID.


### --swift-domain()
Swift domain name.


### --swift-domain-id()
Swift domain ID.
