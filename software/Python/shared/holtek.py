#!/usr/bin/env python3

import time

import usb.core
import usb.util


DEFAULT_VID = 0x04D9
DEFAULT_PID = 0xB534


def find_device(vid, pid):
    print(f"Looking for HT42B534 at {vid:04X}:{pid:04X}...")

    dev = usb.core.find(
        idVendor=vid,
        idProduct=pid
    )

    if dev is None:
        print("Device not found.")
        return None

    print(
        f"Found USB device "
        f"{dev.idVendor:04X}:{dev.idProduct:04X}"
    )

    return dev


def prepare_device(dev):
    # Detach kernel HID driver if necessary.
    try:
        if dev.is_kernel_driver_active(0):
            dev.detach_kernel_driver(0)
            print("Detached kernel HID driver.")
    except (NotImplementedError, usb.core.USBError):
        pass

    try:
        dev.set_configuration()
    except usb.core.USBError as e:
        print(f"Could not set USB configuration: {e}")
        raise


def send_hid_report(dev, report):
    """
    HT42B534 HID configuration transfer.

    bmRequestType = 0x21
    bRequest      = 0x09
    wValue        = 0x0300
    wIndex        = 0x0002
    """

    dev.ctrl_transfer(
        0x21,
        0x09,
        0x0300,
        0x0002,
        report
    )


def enter_bridge_configuration(dev):
    """
    Tell the HT42B534 that we are entering bridge
    configuration mode.
    """

    report = bytearray(512)

    # "bridge-u"
    command = b"bridge-u"

    report[0:len(command)] = command

    send_hid_report(dev, report)


def put_usb_string(report, offset, text):
    """
    Write a USB UTF-16LE string descriptor into the
    configuration report.
    """

    # USB string descriptor length.
    report[offset] = (len(text) + 1) * 2

    # USB string descriptor type.
    report[offset + 1] = 0x03

    for i, char in enumerate(text):
        report[offset + 2 + (i * 2)] = ord(char)
        report[offset + 3 + (i * 2)] = 0


def build_configuration(
    vid,
    pid,
    manufacturer_name,
    product_description,
    serial,
    hardware_flow_control,
    remote_wakeup=False
):
    report = bytearray(512)

    #
    # VID
    #
    report[0] = 0x01
    report[2] = vid & 0xFF
    report[3] = (vid >> 8) & 0xFF

    #
    # PID
    #
    report[4] = pid & 0xFF
    report[5] = (pid >> 8) & 0xFF

    #
    # Manufacturer
    #
    put_usb_string(
        report,
        0x06,
        manufacturer_name
    )

    #
    # Product description
    #
    put_usb_string(
        report,
        0x28,
        product_description
    )

    #
    # Serial number
    #
    put_usb_string(
        report,
        0x6A,
        serial
    )

    #
    # Hardware flow control
    #
    report[0x80] = 0x01 if hardware_flow_control else 0x00

    #
    # Remote wakeup
    #
    report[0x82] = 0x01 if remote_wakeup else 0x00

    #
    # Configuration update flag
    #
    report[0xE2] = 0x01

    return report


def program_device(
    dev,
    new_vid,
    new_pid,
    manufacturer_name,
    product_description,
    serial,
    hardware_flow_control,
    remote_wakeup
):
    print("Entering HT42B534 configuration mode...")

    enter_bridge_configuration(dev)

    time.sleep(0.05)

    report = build_configuration(
        new_vid,
        new_pid,
        manufacturer_name,
        product_description,
        serial,
        hardware_flow_control,
        remote_wakeup
    )

    print()
    print("Programming:")
    print(f"  VID:              0x{new_vid:04X}")
    print(f"  PID:              0x{new_pid:04X}")
    print(f"  Manufacturer:     {manufacturer_name}")
    print(f"  Product:          {product_description}")
    print(f"  Serial:            {serial}")
    print(
        f"  Hardware flow:    "
        f"{'ENABLED' if hardware_flow_control else 'DISABLED'}"
    )
    print(
        f"  Remote wakeup:    "
        f"{'ENABLED' if remote_wakeup else 'DISABLED'}"
    )
    print()

    send_hid_report(dev, report)

    print("Configuration transfer sent.")
    print()
    print(
        "The HT42B534 should now disconnect/re-enumerate "
        "with the new USB descriptors."
    )


def provision_holtek(
    new_vid,
    new_pid,
    manufacturer_name,
    product_description,
    serial,
    hardware_flow_control=True,
    remote_wakeup=False,
    device_vid=DEFAULT_VID,
    device_pid=DEFAULT_PID
):
    dev = find_device(
        device_vid,
        device_pid
    )

    if dev is None:
        return 1

    try:
        prepare_device(dev)

        program_device(
            dev=dev,
            new_vid=new_vid,
            new_pid=new_pid,
            manufacturer_name=manufacturer_name,
            product_description=product_description,
            serial=serial,
            hardware_flow_control=hardware_flow_control,
            remote_wakeup=remote_wakeup
        )

    except usb.core.USBError as e:
        print(f"USB error: {e}")
        return 2

    return 0
