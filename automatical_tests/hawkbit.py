import requests
import json
from base64 import b64encode

class HawkbitManagementAPI:
    def __init__(self, address, username, password, ca_cert=None):
        """
        Initialize the HawkbitManagementAPI instance.

        :param address: The Hawkbit server address.
        :param username: The username for basic authentication.
        :param password: The password for basic authentication.
        :param ca_cert: Optional CA certificate for verifying the server TLS certificate.
        """
        self.address = address
        self.username = username
        self.password = password
        self.ca_cert = ca_cert
        self.headers = {
            "Authorization": self.basic_auth(),
            "Content-Type": "application/json",
        }

    def basic_auth(self):
        """
        Generate a basic authentication token.

        :return: The basic authentication token.
        """
        token = b64encode(f"{self.username}:{self.password}".encode("utf-8")).decode("ascii")
        return f"Basic {token}"

    def make_request(self, method, url, headers=None, **kwargs):
        """
        Make an HTTP request.

        :param method: The HTTP method (GET, POST, PUT, etc.).
        :param url: The URL for the request.
        :param headers: Optional headers for the request.
        :param kwargs: Additional arguments for the request.
        :return: The response object if the request was successful, None otherwise.
        """
        if headers is None:
            headers = self.headers
        response = requests.request(method, url, headers=headers, verify=self.ca_cert, **kwargs)
        if response.status_code in [200, 201]:
            return response
        else:
            print(f"Failed request: {response.status_code} - {response.text}")
            return None

    def register_device(self, target_id, auth_token):
        """
        Register a device with the Hawkbit server.

        :param target_id: The target ID of the device.
        :param auth_token: The authentication token for the device.
        """
        url = f"{self.address}/controller/v1/{target_id}/registration"
        data = {"mode": "merge", "data": {"controllerId": target_id, "tenant": "default"}}
        headers = {
            "Authorization": f"TargetToken {auth_token}",
            "Content-Type": "application/json",
        }
        response = self.make_request("PUT", url, headers=headers, json=data)
        if response:
            print("Device registered successfully")

    def add_binary_file(self, target_id, file_path, auth_token):
        """
        Add a binary file to the Hawkbit server.

        :param target_id: The target ID of the device.
        :param file_path: The path to the binary file.
        :param auth_token: The authentication token for the device.
        """
        url = f"{self.address}/controller/v1/{target_id}/softwaremodules"
        files = {"file": open(file_path, "rb")}
        headers = {
            "Authorization": f"TargetToken {auth_token}",
        }
        response = self.make_request("POST", url, headers=headers, files=files)
        if response:
            print("Binary file added successfully")

    def hawkbit_create_device(self, serial_number):
        """
        Create a new device on the Hawkbit server.

        :param serial_number: The serial number of the device.
        :return: The security token for the new device.
        """
        url = f"{self.address}/rest/v1/targets"
        data = [
            {
                "controllerId": f"AAD_{serial_number:08}",
                "name": f"AAD_{serial_number}",
                "description": "Automatic Alcohol Distilator",
            }
        ]
        response = self.make_request("POST", url, json=data)
        if response:
            response_json = response.json()
            securityToken = response_json[0]["securityToken"]
            print(securityToken)
            return securityToken

    def create_software_module(self, vendor, name, description, module_type, version):
        """
        Create a new software module on the Hawkbit server.

        :param vendor: The vendor of the software module.
        :param name: The name of the software module.
        :param description: The description of the software module.
        :param module_type: The type of the software module.
        :param version: The version of the software module.
        """
        url = f"{self.address}/rest/v1/softwaremodules"
        data = [
            {
                "vendor": vendor,
                "name": name,
                "description": description,
                "type": module_type,
                "version": version
            }
        ]
        response = self.make_request("POST", url, json=data)
        if response:
            print("Software module created successfully")

    def upload_artifact(self, module_id, file_path):
        """
        Upload an artifact to a software module on the Hawkbit server.

        :param module_id: The ID of the software module.
        :param file_path: The path to the artifact file.
        """
        url = f"{self.address}/rest/v1/softwaremodules/{module_id}/artifacts"
        files = {
            "file": open(file_path, "rb")
        }
        response = self.make_request("POST", url, files=files)
        if response:
            print("Artifact uploaded successfully")

    def create_distribution_set(self, name, description, ds_type, version, required_migration_step=False):
        """
        Create a new distribution set on the Hawkbit server.

        :param name: The name of the distribution set.
        :param description: The description of the distribution set.
        :param ds_type: The type of the distribution set.
        :param version: The version of the distribution set.
        :param required_migration_step: Whether a migration step is required.
        """
        url = f"{self.address}/rest/v1/distributionsets"
        data = [
            {
                "requiredMigrationStep": required_migration_step,
                "name": name,
                "description": description,
                "type": ds_type,
                "version": version
            }
        ]
        response = self.make_request("POST", url, json=data)
        if response:
            print("Distribution set created successfully")

    def assign_software_module(self, distribution_id, module_id):
        """
        Assign a software module to a distribution set on the Hawkbit server.

        :param distribution_id: The ID of the distribution set.
        :param module_id: The ID of the software module.
        """
        url = f"{self.address}/rest/v1/distributionsets/{distribution_id}/assignedSM"
        data = [
            {
                "id": str(module_id)
            }
        ]
        response = self.make_request("POST", url, json=data)
        if response:
            print("Software module assigned successfully")

    def assign_distribution_set(self, distribution_id, target_id, assignment_type="forced"):
        """
        Assign a distribution set to a target on the Hawkbit server.

        :param distribution_id: The ID of the distribution set.
        :param target_id: The ID of the target.
        :param assignment_type: The type of assignment (default is "forced").
        """
        url = f"{self.address}/rest/v1/distributionsets/{distribution_id}/assignedTargets/"
        data = [
            {
                "id": target_id,
                "type": assignment_type
            }
        ]
        response = self.make_request("POST", url, json=data)
        if response:
            print("Distribution set assigned successfully")

    def poll_for_updates(self, controller_id, target_token):
        """
        Poll for updates from the Hawkbit server.

        :param controller_id: The ID of the controller.
        :param target_token: The target token for authentication.
        """
        url = f"{self.address}/controller/v1/{controller_id}"
        headers = {
            "Accept": "application/hal+json",
            "Authorization": f"TargetToken {target_token}"
        }
        response = self.make_request("GET", url, headers=headers)
        if response:
            print("Polled for updates successfully")
            print(response.json())

    def inspect_deployment_action(self, controller_id, action_id, target_token):
        """
        Inspect a deployment action on the Hawkbit server.

        :param controller_id: The ID of the controller.
        :param action_id: The ID of the action.
        :param target_token: The target token for authentication.
        """
        url = f"{self.address}/controller/v1/{controller_id}/deploymentBase/{action_id}"
        headers = {
            "Accept": "application/hal+json",
            "Authorization": f"TargetToken {target_token}"
        }
        response = self.make_request("GET", url, headers=headers)
        if response:
            print("Deployment action inspected successfully")
            print(response.json())

    def download_artifact(self, controller_id, module_id, filename, target_token):
        """
        Download an artifact from the Hawkbit server.

        :param controller_id: The ID of the controller.
        :param module_id: The ID of the software module.
        :param filename: The name of the file to save the artifact as.
        :param target_token: The target token for authentication.
        """
        url = f"{self.address}/controller/v1/{controller_id}/softwaremodules/{module_id}/artifacts/{filename}"
        headers = {
            "Authorization": f"TargetToken {target_token}"
        }
        response = self.make_request("GET", url, headers=headers)
        if response:
            with open(filename, 'wb') as file:
                file.write(response.content)
            print(f"Artifact {filename} downloaded successfully")

    def enable_gateway_token_authentication(self):
        """
        Enable gateway token authentication on the Hawkbit server.
        """
        url = f"{self.address}/rest/v1/system/configs/authentication.gatewaytoken.enabled"
        data = {
            "value": True
        }
        response = self.make_request("PUT", url, json=data)
        if response:
            print("Gateway token authentication enabled successfully")

    def initialize_gateway_token(self, token_value):
        """
        Initialize the gateway token on the Hawkbit server.

        :param token_value: The value of the gateway token.
        """
        url = f"{self.address}/rest/v1/system/configs/authentication.gatewaytoken.key"
        data = {
            "value": token_value
        }
        response = self.make_request("PUT", url, json=data)
        if response:
            print("Gateway token initialized successfully")

    def create_rollout(self, distribution_set_id, name, description, target_filter_query, groups):
        """
        Create a new rollout on the Hawkbit server.

        :param distribution_set_id: The ID of the distribution set.
        :param name: The name of the rollout.
        :param description: The description of the rollout.
        :param target_filter_query: The target filter query for the rollout.
        :param groups: The groups for the rollout.
        """
        url = f"{self.address}/rest/v1/rollouts"
        data = {
            "distributionSetId": distribution_set_id,
            "targetFilterQuery": target_filter_query,
            "name": name,
            "description": description,
            "groups": groups
        }
        response = self.make_request("POST", url, json=data)
        if response:
            print("Rollout created successfully")

    def start_rollout(self, rollout_id):
        """
        Start a rollout on the Hawkbit server.

        :param rollout_id: The ID of the rollout.
        """
        url = f"{self.address}/rest/v1/rollouts/{rollout_id}/start"
        response = self.make_request("POST", url)
        if response:
            print("Rollout started successfully")

    def create_target_filter(self, query, name):
        """
        Create a new target filter on the Hawkbit server.

        :param query: The query for the target filter.
        :param name: The name of the target filter.
        """
        url = f"{self.address}/rest/v1/targetfilters"
        data = {
            "query": query,
            "name": name
        }
        response = self.make_request("POST", url, json=data)
        if response:
            print("Target filter created successfully")

    def set_auto_assignment_distribution(self, target_filter_id, distribution_set_id):
        """
        Set auto-assignment for a distribution set on the Hawkbit server.

        :param target_filter_id: The ID of the target filter.
        :param distribution_set_id: The ID of the distribution set.
        """
        url = f"{self.address}/rest/v1/targetfilters/{target_filter_id}/autoAssignDS"
        data = {
            "id": str(distribution_set_id)
        }
        response = self.make_request("POST", url, json=data)
        if response:
            print("Auto assignment distribution set successfully")

if __name__ == "__main__":
    api = HawkbitManagementAPI("http://192.168.1.2:8090", "admin", "admin")
    # securityToken = api.hawkbit_create_device(1234567890)
    # print(securityToken)
    # api.register_device("Bimbrownik1", "1234567890")
    api.add_binary_file("Bimbrownik1", "../build/bimbrownik.bin", "1234567890")
    api.create_software_module("Example Ltd.", "MyOS", "First version of MyOS.", "os", "1.0")
    api.upload_artifact(1, "path/to/your/artifact01.file")
    api.create_distribution_set("MyDS", "My initial distribution", "os", "1.0")
    api.assign_software_module(1, 1)
    api.assign_distribution_set(1, "dev01")
    api.poll_for_updates("dev01", "REPLACE_WITH_TARGET_TOKEN")
    api.inspect_deployment_action("dev01", 1, "REPLACE_WITH_TARGET_TOKEN")
    api.download_artifact("dev01", 1, "artifact01.file", "REPLACE_WITH_TARGET_TOKEN")
    api.enable_gateway_token_authentication()
    api.initialize_gateway_token("e61c6b2b78a674d19304c357a20f1d09")
    groups = [
        {
            "name": "EMEA_Devices",
            "description": "Devices in EMEA",
            "targetFilterQuery": "name==emea*",
            "successCondition": {
                "condition": "THRESHOLD",
                "expression": "70"
            },
            "successAction": {
                "expression": "",
                "action": "NEXTGROUP"
            },
            "errorAction": {
                "expression": "",
                "action": "PAUSE"
            },
            "errorCondition": {
                "condition": "THRESHOLD",
                "expression": "20"
            }
        },
        {
            "name": "APAC_Devices",
            "description": "Devices in APAC",
            "targetFilterQuery": "name==apac*",
            "successCondition": {
                "condition": "THRESHOLD",
                "expression": "50"
            },
            "successAction": {
                "expression": "",
                "action": "NEXTGROUP"
            },
            "errorAction": {
                "expression": "",
                "action": "PAUSE"
            },
            "errorCondition": {
                "condition": "THRESHOLD",
                "expression": "20"
            }
        },
        {
            "name": "AMER_Devices",
            "description": "Devices in AMER",
            "targetFilterQuery": "name==amer*",
            "successCondition": {
                "condition": "THRESHOLD",
                "expression": "25"
            },
            "successAction": {
                "expression": "",
                "action": "NEXTGROUP"
            },
            "errorAction": {
                "expression": "",
                "action": "PAUSE"
            },
            "errorCondition": {
                "condition": "THRESHOLD",
                "expression": "20"
            }
        }
    ]
    api.create_rollout(1, "MyOS-Global-Rollout-1.0", "Global rollout of MyOS", "description=='Plug and Play*'", groups)
    api.start_rollout(1)
    api.create_target_filter("name==emeadevice*", "EMEA_Devices")
    api.set_auto_assignment_distribution(1, 1)
