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
        cert = "-----BEGIN CERTIFICATE-----\n" + "M" * 5000 + "\n-----END CERTIFICATE-----"

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
