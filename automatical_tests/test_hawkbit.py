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
        self.hawkbit_api = HawkbitManagementAPI(
            self.hawkbit_address, self.hawkbit_username, self.hawkbit_password
        )
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
        print(f"Serial number: {serial_number}")

        # Verify if the device is registered in Hawkbit server
        target_id = f"{serial_number}"
        url = f"{self.hawkbit_api.address}/rest/v1/targets/{target_id}"

        for _ in range(3):
            response = self.hawkbit_api.make_request("GET", url)
            if response is not None:
                break
            time.sleep(5)
        else:
            assert False, f"Device {target_id} is not registered in Hawkbit server"

    def test_add_binary_file_and_update_device(self):
        version = "1.0.18"
        status, serial_number = self.dev.get_serial_number_config()
        assert status == 200, f"Failed to get serial number: {serial_number}"
        LOGGER.info(f"Serial number: {serial_number}")

        # Step 1: Create a software module
        module_id = self.hawkbit_api.create_software_module(
            "HQC", "TestOS", "TestOS", "os", version
        )
        assert module_id is not None, "Failed to create software module"

        # Step 2: Upload an artifact
        file_path = "../build/bimbrownik.bin"
        self.hawkbit_api.upload_artifact(module_id, file_path)

        # Step 3: Create a distribution set
        distribution_id = self.hawkbit_api.create_distribution_set(
            "TestDS", "Distribution for automated tests", "os", version
        )
        assert distribution_id is not None, "Failed to create distribution set"

        # Step 4: Assign the software module to the distribution set
        self.hawkbit_api.assign_software_module(distribution_id, module_id)

        # Step 5: Assign the distribution set to the device
        self.hawkbit_api.assign_distribution_set(distribution_id, serial_number)

        # Step 6: Reboot the device for polling the update
        self.dev.restart_device()

        action_id = self.hawkbit_api.get_target_action(serial_number)

        # Step 7: Provide intermediary feedback and wait to finish update during 5 minutes
        start_time = time.time()
        reboot = False
        update = False
        while time.time() - start_time < 300:
            action_details = self.hawkbit_api.get_action_by_id(serial_number, action_id)
            LOGGER.info(f"Action details: {action_details}")
            if action_details:
                content = action_details.get("content", [])
                for action in content:
                    if action.get("type") == "error":
                        assert False, f"Update failed: {action.get('messages', [])}"
                    if action.get("type") == "finished":
                        print("Device updated successfully")
                        update = True
                        break
                    if action.get("type") == "running" and reboot == False:
                        for messages in action.get("messages", []):
                            if "reboot" in messages:
                                reboot = True
                                self.dev.restart_device()
                                break
                        break
            if update:
                break
            time.sleep(5)
        else:
            assert False, "Device update failed after 5 minutes"

        # Unassign the software module from the distribution set and delete them after the test
        self.hawkbit_api.unassign_software_module(distribution_id, module_id)
        self.hawkbit_api.delete_distribution_set(distribution_id)
        self.hawkbit_api.delete_software_module(module_id)


if __name__ == "__main__":
    pytest.main()
