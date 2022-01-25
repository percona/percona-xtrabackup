import ssl
import logging
import os
from kmip.pie.client import ProxyKmipClient, enums
from kmip.core.factories import attributes

logging.basicConfig(encoding='utf-8', level=logging.DEBUG)

client = ProxyKmipClient(
#        config_file="/home/dutow/workspace/topics/ps7586/8.0/pykmip.conf", 
        kmip_version=enums.KMIPVersion.KMIP_2_0,
        ssl_version="PROTOCOL_SSLv23",
        cert=os.environ.get("KMIP_CLIENT_CA"),
        key=os.environ.get("KMIP_CLIENT_KEY"),
        ca=os.environ.get("KMIP_SERVER_CA"),
        config='client',
        hostname=os.environ.get("KMIP_ADDR"),
        port=os.environ.get("KMIP_PORT")
        )

f = attributes.AttributeFactory()

with client:
    l = client.locate(
            attributes=[
                f.create_attribute(
                    enums.AttributeType.OBJECT_TYPE,
                    enums.ObjectType.SYMMETRIC_KEY
                )
            ]
    )
    for id in l:
        client.destroy(id)

