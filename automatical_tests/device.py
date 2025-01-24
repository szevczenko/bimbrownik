import requests
import serial
from scan_devices import ScanDevices
import time
import json
import pywifi
from pywifi import const


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

    def get_serial_number_config(self):
        r"""Get serial number configuration from device

        Returns a tuple with status code and serial number if operation success. Otherwise return status code and None.
        :rtype: tuple(int, str or None)
        """
        status, value = self.get_device_config()
        if status == 200:
            print(value)
            try:
                config = json.loads(value)
                return status, config.get("sn", None)
            except json.JSONDecodeError:
                return status, None
        return status, None

    def set_serial_number_config(self, serial_number: str, magic_word: str = ""):
        r"""Set serial number configuration on device

        Returns a tuple with status code and response text.
        :rtype: tuple(int, str)
        """
        data = json.dumps({"sn": serial_number, "magic": magic_word})
        return self.http_api_set("dev_config", "serial_number", data)


def host_scan_and_connect_to_device(password: str):
    r"""Scan for available Wi-Fi networks and connect to the first one starting with 'Bimbrownik:'

    Returns a tuple with status code and response text.
    :rtype: tuple(int, str)
    """
    wifi = pywifi.PyWiFi()
    iface = wifi.interfaces()[0]
    iface.scan()
    time.sleep(2)  # Wait for the scan to complete
    scan_results = iface.scan_results()
    for network in scan_results:
        if network.ssid.startswith("Bimbrownik:"):
            iface.disconnect()
            time.sleep(1)
            profile = pywifi.Profile()
            profile.ssid = network.ssid
            profile.auth = const.AUTH_ALG_OPEN
            profile.akm.append(const.AKM_TYPE_WPA2PSK)
            profile.cipher = const.CIPHER_TYPE_CCMP
            profile.key = password
            iface.remove_all_network_profiles()
            tmp_profile = iface.add_network_profile(profile)
            iface.connect(tmp_profile)
            time.sleep(10)  # Wait for the connection to complete
            if iface.status() == const.IFACE_CONNECTED:
                return 200, f"Connected to {network.ssid}"
            else:
                return 400, f"Failed to connect to {network.ssid}"
    return 404, "No Wi-Fi networks starting with 'Bimbrownik:' found"


def device_scan_and_connect(ssid: str, password: str):
    r"""Scan for available Wi-Fi networks and connect to the specified one using device's Wi-Fi HTTP API

    Returns a tuple with status code and response text.
    :rtype: tuple(int, str)
    """
    with requests.Session() as session:
        # Scan for Wi-Fi networks
        url = "http://10.10.0.1/ap.json"
        response = session.get(url)
        if response.status_code == 200:
            networks = response.json()
            for network in networks:
                print(network)
                if network['ssid'] == ssid:
                    # Connect to the Wi-Fi network
                    connect_url = "http://10.10.0.1/connect.json"
                    headers = {
                        "X-Custom-ssid": network['ssid'],
                        "X-Custom-pwd": password
                    }
                    connect_response = session.post(connect_url, headers=headers)
                    return connect_response.status_code, connect_response.text
            return 404, f"No Wi-Fi networks with SSID '{ssid}' found"
        else:
            return response.status_code, response.text


if __name__ == "__main__":
    # devices = ScanDevices("Production", 3)
    # address = devices[0][0]
    address = "192.168.1.154"
    dev = Device(address, "COM6")

    # Scan and connect to Wi-Fi network
    ssid = "Bimbrownik:Example"
    password = "SuperTrudne1!-_"
    status, response = host_scan_and_connect_to_device(password)
    print(f"Scan and connect result: {status}, {response}")
    status, response = device_scan_and_connect("TP-Link_2AC1", "19681115")
    print(f"Scan and connect result: {status}, {response}")
