from zeroconf import ServiceBrowser, ServiceListener, Zeroconf
import time

_deviceList = []


class DeviceListener(ServiceListener):
    _name = None

    def __init__(self, name):
        self._name = name

    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        print(f"Service {name} updated")

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        print(f"Service {name} removed")

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        info = zc.get_service_info(type_, name)
        if name.find(self._name) != -1:
            address = info.parsed_scoped_addresses()[0]
            port = info.port
            mac = info.properties[b"mac"].decode("utf-8")
            _deviceList.append((address, port, mac))


def ScanDevices(name="Bimbrownik", scanning_time_s=3):
    """Return list of turples: (ip_address: str, port: int)"""
    print("Scanning devices...")
    _deviceList.clear()
    zeroconf = Zeroconf()
    listener = DeviceListener(name)
    browser = ServiceBrowser(zeroconf, "_remote._http.local.", listener)
    time.sleep(scanning_time_s)
    zeroconf.close()
    return _deviceList


if __name__ == "__main__":
    result = ScanDevices(name="Bimbrownik")
    print(result)
