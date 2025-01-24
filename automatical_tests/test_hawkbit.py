import pytest
import device
from hawkbit import HawkbitManagementAPI
import time
import scan_devices
import logging

LOGGER = logging.getLogger(__name__)

class TestClassHawkbit:
    @pytest.fixture(scope="class", autouse=True)
    def setup_class(self, request):
        self.device_address = "192.168.1.154"
        self.device_port = 80
        self.hawkbit_address = "http://192.168.1.2:8090"
        self.hawkbit_username = "admin"
        self.hawkbit_password = "admin"
        self.hawkbit_api = HawkbitManagementAPI(self.hawkbit_address, self.hawkbit_username, self.hawkbit_password)
        self._scan()

        # Attach the instance to the request to make it accessible in tests
        request.cls.hawkbit_api = self.hawkbit_api
        request.cls.dev = self.dev

    def _scan(self):
        for _ in range(3):
            self.devices = scan_devices.ScanDevices()
            if len(self.devices) > 0:
                self.dev = device.Device(self.devices[0][0], self.devices[0][1])
                return
        assert False, "Failed to scan for devices"

    def _restart(self):
        LOGGER.info("Restarting the device")
        status, value = self.dev.restart_device()
        assert status == 200 and value == "OK"
        time.sleep(5)
        self._scan()

    def test_configure_device_for_hawkbit(self):
        # Configure device to connect to Hawkbit server
        hawkbit_config = {
            "address": self.hawkbit_api.address,
            "tenant": "default",
            "tls": "false",
            "poll_time": "60",
            "token": "1234567890",
        }

        for key, value in hawkbit_config.items():
            status, response = self.dev.set_hawkbit_config(key, value)
            assert status == 200, f"Failed to set Hawkbit config {key}: {response}"

        # Save configuration
        status, response = self.dev.http_api_set("hawkbit", "save", "")
        assert status == 200, f"Failed to save Hawkbit configuration: {response}"

        # Restart the device
        self._restart()

    def test_verify_device_registration_in_hawkbit(self):
        # Get the serial number of the device
        status, serial_number = self.dev.get_serial_number_config()
        assert status == 200, f"Failed to get serial number: {serial_number}"

        # Verify if the device is registered in Hawkbit server
        target_id = f"AAD_{serial_number:08}"
        url = f"{self.hawkbit_api.address}/rest/v1/targets/{target_id}"

        for _ in range(3):
            response = self.hawkbit_api.make_request("GET", url)
            if response is not None:
                break
            time.sleep(5)
        else:
            assert False, f"Device {target_id} is not registered in Hawkbit server"

if __name__ == "__main__":
    pytest.main()
