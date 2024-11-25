import os
import can

def get_can_adapter(dev='can0', bitrate=1000000):
    os.system(f'sudo ip link set {dev} down')
    os.system(f'sudo ip link set {dev} type can bitrate {bitrate}')
    os.system(f'sudo ip link set {dev} up')

    bus = can.Bus(channel=dev, interface='socketcan')

    return bus

def int_base_auto(value):
    # Remove any leading or trailing whitespace
    value = value.strip()

    # Detect based on prefix
    if value.startswith(('0x', '0X')):
        # Hexadecimal
        return int(value, 16)
    elif value.startswith(('0b', '0B')):
        # Binary
        return int(value, 2)
    elif value.startswith(('0o', '0O')):
        # Octal (explicit in Python 3 with 0o prefix)
        return int(value, 8)
    elif value.startswith('0') and len(value) > 1:
        # Leading 0 but no 0o or 0x, likely octal (Python 2-style octal)
        return int(value, 8)
    else:
        # Default to decimal
        return int(value, 10)
    
