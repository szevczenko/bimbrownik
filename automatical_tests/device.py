import requests
import serial
from scan_devices import ScanDevices
import time
import json


class Device:
    def __init__(self, address: str = None, port: int = 80, mac: str = None):
        self.address = address
        self.port = port
        self.mac = mac

    def get_serial_number(self):
        r"""Get serial number from device

        Returns serial number if operation success. Otherwise return None.
        :rtype: str or None
        """
        with requests.Session() as session:
            url = f"http://{self.address}:{self.port}/api/sn"
            response = session.get(url)
            if response.status_code == 200:
                return response.text
            else:
                return None

    def http_api_get(self, api_name: str, config_name: str):
        r"""Generic method to get configuration from device using HTTP API

        Returns a tuple with status code and configuration value if operation success. Otherwise return status code and None.
        :param api_name: The name of the API to call.
        :param config_name: The name of the configuration to get.
        :rtype: tuple(int, str or None)
        """
        with requests.Session() as session:
            url = f"http://{self.address}:{self.port}/api/{api_name}/{config_name}"
            response = session.get(url)
            if response.status_code == 200:
                return response.status_code, response.text
            else:
                return response.status_code, response.text

    def http_api_set(self, api_name: str, config_name: str, config_value: str):
        r"""Generic method to set configuration on device using HTTP API

        Returns a tuple with status code and response text.
        :param api_name: The name of the API to call.
        :param config_name: The name of the configuration to set.
        :param config_value: The value of the configuration to set.
        :rtype: tuple(int, str)
        """
        with requests.Session() as session:
            url = f"http://{self.address}:{self.port}/api/{api_name}/{config_name}"
            response = session.post(url, data=str(config_value))
            return response.status_code, response.text

    def get_hawkbit_config(self, config_name: str):
        r"""Get Hawkbit configuration from device

        Returns a tuple with status code and configuration value if operation success. Otherwise return status code and None.
        :rtype: tuple(int, str or None)
        """
        return self.http_api_get("hawkbit", config_name)

    def set_hawkbit_config(self, config_name: str, config_value: str):
        r"""Set Hawkbit configuration on device

        Returns a tuple with status code and response text.
        :rtype: tuple(int, str)
        """
        return self.http_api_set("hawkbit", config_name, config_value)
    
    def put_hawkbit_config(self, config_name: str, config_value: str):
        r"""Put Hawkbit configuration on device

        Returns a tuple with status code and response text.
        :rtype: tuple(int, str)
        """
        with requests.Session() as session:
            url = f"http://{self.address}:{self.port}/api/hawkbit/{config_name}"
            response = session.put(url, data=str(config_value))
            return response.status_code, response.text

    def get_mqtt_config(self, config_name: str):
        r"""Get MQTT configuration from device

        Returns a tuple with status code and configuration value if operation success. Otherwise return status code and None.
        :rtype: tuple(int, str or None)
        """
        return self.http_api_get("mqtt", config_name)

    def set_mqtt_config(self, config_name: str, config_value: str):
        r"""Set MQTT configuration on device

        Returns a tuple with status code and response text.
        :rtype: tuple(int, str)
        """
        return self.http_api_set("mqtt", config_name, config_value)

    def get_device_config(self):
        r"""Get device configuration

        Returns a tuple with status code and configuration value if operation success. Otherwise return status code and None.
        :rtype: tuple(int, str or None)
        """
        return self.http_api_get("dev_config", "")

    def restart_device(self):
        r"""Restart the device

        Returns a tuple with status code and response text.
        :rtype: tuple(int, str)
        """
        return self.http_api_set("dev_config", "restart", "")


if __name__ == "__main__":
    # devices = ScanDevices("Production", 3)
    # address = devices[0][0]
    address = "192.168.1.154"
    dev = Device(address, "COM6")
