#! /usr/bin/env python3

import can
import os
import time
import struct
import argparse
from ctypes import *
from math import *
from util import *

# Add the following line to /etc/sudoers.d/network_interface
# ALL ALL=(ALL) NOPASSWD: /usr/bin/ip link set can0 *

class bootloader_writer:
    def __init__(self, device_id, bus):
        self.bus = bus
        self.device_id = device_id

        self.PAGE_SIZE = 0x800

        self.CMD_MASK   = 0x01000000
        self.REPLY_MASK = 0x08000000
        self.ERROR_MASK = 0x00800000
        self.BASE_CMD   = 0x10000000 + device_id

    def __del__(self):
        self.bus.shutdown()

    def clear_bus(self):
        while self.bus.recv(0):
            pass
    
    def wait_for_reply(self, cmd, timeout = 1):
        t = time.time()
        while time.time() - t < timeout:
            msg = self.bus.recv(0)
            if msg is None:
                continue

            cmd |= self.REPLY_MASK

            id = msg.arbitration_id

            if id == cmd:
                is_valid = (msg.arbitration_id & self.ERROR_MASK) == 0
                return is_valid, msg
            
        return None
    
    def read_binary_data(self, bindata, position):
        
        length = len(bindata) - position

        if length <= 0:
            return None
        

        if length < 8:
            bytes_littleendian = bindata[position:] + b'\xff' * (8 - length)
        else:
            bytes_littleendian = bindata[position:position + 8]

        # Unpack the data as two little-endian 32-bit unsigned ints
        bytes_bigendian = struct.pack('>II',
                *struct.unpack('<II', bytes_littleendian))

        return bytes_bigendian
    
    def send_cmd(self, cmd, data, timeout = 0.1):
        self.clear_bus()
        while True:
            try:
                self.bus.send(can.Message(arbitration_id=cmd, data=data))
            except:
                pass

            reply = self.wait_for_reply(cmd, timeout)
            if reply is not None:
                return reply
    
    def ping(self):
        cmd = self.BASE_CMD | self.CMD_MASK * 0
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def pagesel(self, page):
        page <<= 8
        cmd = self.BASE_CMD | page | self.CMD_MASK * 1
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def lock(self):
        cmd = self.BASE_CMD | self.CMD_MASK * 2
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def unlock(self):
        cmd = self.BASE_CMD | self.CMD_MASK * 3
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def erase(self, page):
        page <<= 8
        cmd = self.BASE_CMD | page | self.CMD_MASK * 4
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def write(self, addr, data):
        addr = (addr % self.PAGE_SIZE) << 5
        cmd = self.BASE_CMD | addr | self.CMD_MASK * 5
        return self.send_cmd(cmd, data)
    
    def read(self, addr):
        addr = (addr % self.PAGE_SIZE) << 5
        cmd = self.BASE_CMD | addr | self.CMD_MASK * 6
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def finish(self):
        cmd = self.BASE_CMD | self.CMD_MASK * 7
        return self.send_cmd(cmd, b'\x00\x00\x00\x00\x00\x00\x00\x00')
    
    def write_file(self, bindata, page_offset):
        print(f'Pinging device 0x{self.device_id:02x}...')
        self.ping()
        if not self.unlock()[0]:
            print(f'Failed to unlock device 0x{self.device_id:02x}')
            return False
        
        page = page_offset - 1
        position = 0

        while True:
            data = self.read_binary_data(bindata, position)

            if data is None:
                break

            newpage = (position // self.PAGE_SIZE) + page_offset
            if (newpage != page):
                page = newpage
                print(f'Writing page {page:3d} to device 0x{self.device_id:02x}...')
                if not self.pagesel(page)[0]:
                    print(f'Failed to select page {page:3d}')
                    return False

                if not self.erase(page)[0]:
                    print(f'Failed to erase page {page:3d}')
                    return False

            ret, msg = self.write(position, data)
            if not ret or msg.data != data:
                realpos = position + page_offset * self.PAGE_SIZE
                print(f'Failed to write data at position {realpos}')
                return False
            
            position += 8

        
        if not self.lock()[0]:
            print(f'Failed to lock device 0x{self.device_id:02x}')
            return False
        
        if not self.finish()[0]:
            print(f'Failed to finish writing')
            return False
        
        print(f'Finished writing to device 0x{self.device_id:02x}')

def main():
    parser = argparse.ArgumentParser(description='Upload a binary file to a bootloader')
    parser.add_argument('--id', type=int_base_auto, default = 0xff, help='The device ID to upload to')
    parser.add_argument('--filename', '-f', type=str, help='The binary file to upload')
    parser.add_argument('--bootloader', '-b', type=str, default='bootloader.bin', help='The bootloader file to upload')
    parser.add_argument('--page_offset', type=int, default=2, help='The page offset to use')
    parser.add_argument('--last-page', type=int, default=63, help='The last page in flash')
    parser.add_argument('--dev', type=str, default='can0', help='The CAN device to use')
    parser.add_argument('--bitrate', type=int, default=125000, help='The CAN bitrate to use')
    parser.add_argument('--stlink', action='store_true', help='Use ST-Link to program')
    parser.add_argument('--erase', '-e', action='store_true', help='Erase the flash before programming')

    args = parser.parse_args()

    if args.stlink:
        if args.erase:
            os.system(f'st-flash erase')
            os.system(f'st-flash write {args.bootloader} 0x08000000')
            os.system(f'st-flash write {args.filename} 0x08001000')
        os.system(f'st-flash reset')
        return


    bus = get_can_adapter(args.dev, args.bitrate)

    writer = bootloader_writer(args.id, bus)

    if args.filename:
        with open(args.filename, 'rb') as f:
            bindata = f.read()

        writer.write_file(bindata, args.page_offset)

    print('Finished programming!')

if __name__ == '__main__':
    main()
