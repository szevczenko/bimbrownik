import scan_devices
import device
import logging
import random
import time

LOGGER = logging.getLogger(__name__)


# content of test_class_demo.py
class TestClassHttpApi:
    value = 0
    target = None

    def _scan(self):
        if self.target is None:
            for i in range(3):
                self.devices = scan_devices.ScanDevices()
                if len(self.devices) > 0:
                    break
            assert len(self.devices) > 0
            self.target = device.Device(self.devices[0][0], self.devices[0][1])

    def _restart(self):
        LOGGER.info("Restarting the device")
        status, value = self.target.restart_device()
        assert status == 200 and value == "OK"
        time.sleep(5)
        self.target = None
        self._scan()

    def test_init_device_wifi_connection(self):
        password = "SuperTrudne1!-_"
        router_ssid = "TP-Link_2AC1"
        router_password = "19681115"

        # Try to connect to the device using host_scan_and_connect_to_device
        status, response = device.host_scan_and_connect_to_device(password)
        if status != 200:
            # If connection fails, try to scan for devices
            self._scan()
            if self.target is None:
                assert False, "Failed to scan and connect to the device"
            else:
                return

        # Try to connect to the router using device_scan_and_connect three times
        for _ in range(3):
            status, response = device.device_scan_and_connect(
                router_ssid, router_password
            )
            if status == 200:
                break
            time.sleep(5)
        else:
            assert False, "Failed to connect to the router using the device"

        # Sleep for 10 seconds
        time.sleep(10)

        # Try to scan for devices again
        self._scan()
        if self.target is None:
            assert (
                False
            ), "Failed to scan and connect to the device after connecting to the router"

    def test_hawkbit_api(self):
        self._scan()
        address = f"http://example{random.randint(0,100)}.com"
        tenant = f"default{random.randint(0,100)}"
        tls = "true"
        poll_time = random.randint(0, 100)
        token = f"1234567890{random.randint(0,100)}"

        # Set configurations
        status, value = self.target.set_hawkbit_config("address", address)
        assert status == 200 and value == "OK"
        status, value = self.target.set_hawkbit_config("tenant", tenant)
        assert status == 200 and value == "OK"
        status, value = self.target.set_hawkbit_config("tls", tls)
        assert status == 200 and value == "OK"
        status, value = self.target.set_hawkbit_config("poll_time", poll_time)
        assert status == 200 and value == "OK"
        status, value = self.target.set_hawkbit_config("token", token)
        assert status == 200 and value == "OK"
        status, value = self.target.set_hawkbit_config("unknown", token)
        assert status == 400 and value == "Parameter not exist"

        # Save configuration
        status, value = self.target.http_api_set("hawkbit", "save", "")
        assert status == 200 and value == "OK"

        # Restart the device
        self._restart()

        # Check if configurations are saved properly
        status, value = self.target.get_hawkbit_config("address")
        assert status == 200
        assert value == address
        status, value = self.target.get_hawkbit_config("tenant")
        assert status == 200
        assert value == tenant
        status, value = self.target.get_hawkbit_config("tls")
        assert status == 200
        assert value == tls
        status, value = self.target.get_hawkbit_config("poll_time")
        assert status == 200
        assert value == str(poll_time)
        status, value = self.target.get_hawkbit_config("token")
        assert status == 200
        assert value == token
        status, value = self.target.put_hawkbit_config("token", 123)
        assert status == 405
        assert value == "Method not allowed"

    def test_mqtt_api(self):
        self._scan()
        address = f"mqtt://example{random.randint(0,100)}.com"
        ssl = "true"
        prefix = f"test/prefix{random.randint(0,100)}"
        data = f"test/data{random.randint(0,100)}"
        user = f"testuser{random.randint(0,100)}"
        password = f"testpass{random.randint(0,100)}"
        cert = (
            "-----BEGIN CERTIFICATE-----\n" + "M" * 5000 + "\n-----END CERTIFICATE-----"
        )

        # Set configurations
        status, value = self.target.set_mqtt_config("address", address)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("ssl", ssl)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("prefix", prefix)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("data", data)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("user", user)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("pass", password)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("cert", cert)
        assert status == 200 and value == "OK"
        status, value = self.target.set_mqtt_config("unknown", password)
        assert status == 400 and value == "Parameter not exist"

        # Save configuration
        status, value = self.target.http_api_set("mqtt", "save", "")
        assert status == 200 and value == "OK"

        # Restart the device
        self._restart()

        # Check if configurations are saved properly
        status, value = self.target.get_mqtt_config("address")
        assert status == 200
        assert value == address
        status, value = self.target.get_mqtt_config("ssl")
        assert status == 200
        assert value == ssl
        status, value = self.target.get_mqtt_config("prefix")
        assert status == 200
        assert value == prefix
        status, value = self.target.get_mqtt_config("data")
        assert status == 200
        assert value == data
        status, value = self.target.get_mqtt_config("user")
        assert status == 200
        assert value == user
        status, value = self.target.get_mqtt_config("pass")
        assert status == 200
        assert value == password
        status, value = self.target.get_mqtt_config("cert")
        assert status == 200
        assert value == cert

    def test_config_api(self):
        self._scan()

        # Test restarting the device
        self._restart()

        # Test getting device configuration
        status, value = self.target.get_device_config()
        assert status == 200
        assert "sw" in value
        assert "project" in value
        assert "sn" in value

    def test_unknown_api(self):
        self._scan()
        status, value = self.target.http_api_get("unknown", "unknown")
        assert status == 400 and value == "Unknown API"

    def test_serial_number_api(self):
        self._scan()
        serial_number = f"SN{random.randint(1000, 9999)}"
        magic_word = "SUPER_GRASS"

        # Check if serial number is already set
        status, value = self.target.get_serial_number_config()
        if status == 200 and value:
            print(f"Serial number already set: {value}")
        else:
            # Set serial number without magic word
            status, value = self.target.set_serial_number_config(serial_number)
            assert status == 200 and value == "OK"

        # Try to set serial number again without magic word
        status, value = self.target.set_serial_number_config(serial_number)
        assert status == 400 and value == "Serial number already set"

        # Set serial number with magic word
        status, value = self.target.set_serial_number_config(serial_number, magic_word)
        assert status == 200 and value == "OK"

        # Get serial number
        status, value = self.target.get_serial_number_config()
        assert status == 200
        assert value == serial_number
