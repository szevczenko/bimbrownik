import os
import sys
import requests
from requests.auth import HTTPBasicAuth
from base64 import b64encode

IDF_PATH = os.getenv("IDF_PATH")
PART_TOOL_PATH = os.path.join(IDF_PATH, "components", "partition_table")
sys.path.append(PART_TOOL_PATH)

from parttool import *


PARTITION_NAME = "dev_config"

DEV_CONF_BIN_FILE = "device_config.bin"
DEV_CONF_CSV_FILE = "device_config.csv"
DEV_CONF_STORAGE_NAME = "config"
DEV_CONF_SN = 5

OTA_CONF_BIN_FILE = "device_config.bin"
OTA_CONF_CSV_FILE = "device_config.csv"
OTA_STORAGE_NAME = "ota_config"

OTA_ADDRESS = "192.168.1.136:8443"
OTA_TENANT = "default"
OTA_TLS = 1
OTA_POLL_TIME = 100

ota_token = "failed get token"


def write_config_to_target(dev="COM9", token=None):
    dev_config_str = f'# AAD csv file\nkey,type,encoding,value\n{DEV_CONF_STORAGE_NAME},namespace,,\nSN,data,u32,{DEV_CONF_SN}\n{OTA_STORAGE_NAME},namespace,,\naddress,data,string,"{OTA_ADDRESS}"\ntenant,data,string,"{OTA_TENANT}"\ntls,data,u8,{OTA_TLS}\ntoken,data,string,"{token}"\npoll_time,data,i32,{OTA_POLL_TIME}'
    f = open(DEV_CONF_CSV_FILE, "w")
    f.write(dev_config_str)
    f.close()
    result = os.system(
        f"python {IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate {DEV_CONF_CSV_FILE} {DEV_CONF_BIN_FILE} 0x3000"
    )
    print(result)
    target = ParttoolTarget(dev)
    target.write_partition(PartitionName(PARTITION_NAME), DEV_CONF_BIN_FILE)

def basic_auth(username, password):
    token = b64encode(f"{username}:{password}".encode('utf-8')).decode("ascii")
    return f'Basic {token}'

def hawk_bit_create_device(serial_number):
    prefix = "http://"
    if OTA_TLS > 0:
        prefix = "https://"
    url = f"{prefix}{OTA_ADDRESS}/rest/v1/targets"
    data = [
        {"controllerId": f"AAD_{serial_number:08}", "name": f"AAD_{serial_number}", "description": "Automatic Alcohol Distilator"}
    ]
    print(data)
    session = requests.Session()
    response = session.post(
        url,
        verify=False,
        headers={
            "Accept": "application/hal+json",
            "Content-Type": "application/json",
            "Authorization":  basic_auth("admin", "test12345678"),
            "Username": "admin",
            "Password": "test12345678"
        },
        json=data
    )
    response_json = response.json()
    securityToken = response_json[0]["securityToken"]
    print(securityToken)
    return securityToken

if __name__ == "__main__":
    securityToken = hawk_bit_create_device(DEV_CONF_SN)
    write_config_to_target("COM9", securityToken)
