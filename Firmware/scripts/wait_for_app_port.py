# After an nrfutil DFU the XIAO reboots and re-enumerates its USB CDC as the
# application port under a new ttyACM* name. `pio run -t upload -t monitor` can
# open the monitor before that happens, so block the upload target until the
# application CDC is back. This avoids unplugging/replugging the board after
# every flash. See platformio/platform-nordicnrf52#206.

from time import sleep, time

from platformio.device.list.util import list_serial_ports

Import("env")

# Seeed XIAO nRF52840 application USB CDC (Seeed VID 0x2886, PID 0x8044).
APP_USB_ID = "2886:8044"
WAIT_SECONDS = 20


def _app_port_present():
    for port in list_serial_ports():
        if APP_USB_ID in (port.get("hwid") or "").upper():
            return True
    return False


def _wait_for_app_port(source, target, env):
    deadline = time() + WAIT_SECONDS
    while time() < deadline:
        if _app_port_present():
            print("[wait_for_app_port] application CDC is back")
            return
        sleep(0.2)
    print("[wait_for_app_port] warning: application CDC did not reappear")


env.AddPostAction("upload", _wait_for_app_port)
