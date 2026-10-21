"""Loopback-only robot protocol simulator. No physical robot control."""
import argparse
import socket
import struct
import threading
import time

MAGIC = b'\x5a\x5a'
MAX_PAYLOAD = 4096

def crc16(data):
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc

def pack(kind, payload=b''):
    data = MAGIC + struct.pack('<HI', kind, len(payload)) + payload
    return data + struct.pack('<H', crc16(data))

def extract(buffer):
    frames = []
    while len(buffer) >= 8:
        if buffer[:2] != MAGIC:
            del buffer[0]
            continue
        kind, length = struct.unpack_from('<HI', buffer, 2)
        if length > MAX_PAYLOAD:
            del buffer[0]
            continue
        total = 10 + length
        if len(buffer) < total:
            break
        if crc16(buffer[:total-2]) != struct.unpack_from('<H', buffer, total-2)[0]:
            del buffer[0]
            continue
        frames.append((kind, bytes(buffer[8:total-2])))
        del buffer[:total]
    return frames

class MockRobot:
    def __init__(self, mode='normal', port=0):
        self.mode = mode
        self.stop_event = threading.Event()
        self.listener = socket.socket()
        self.listener.bind(('127.0.0.1', port))
        self.listener.listen(4)
        self.listener.settimeout(0.1)
        self.port = self.listener.getsockname()[1]
        self.connections = 0
        self.commands = 0
        self.error = None
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self):
        self.thread.start()
        return self

    def _run(self):
        try:
            if self.mode == 'delayed':
                # Bind reserves the port; close it first in tests for true delayed startup.
                self.stop_event.wait(0.5)
            while not self.stop_event.is_set():
                try:
                    conn, _ = self.listener.accept()
                except socket.timeout:
                    continue
                self.connections += 1
                with conn:
                    conn.settimeout(0.1)
                    buffer = bytearray()
                    began = time.monotonic()
                    injected = False
                    while not self.stop_event.is_set():
                        if self.mode == 'reconnect' and time.monotonic()-began > 0.3:
                            break
                        try:
                            data = conn.recv(1024)
                        except socket.timeout:
                            continue
                        except (ConnectionResetError, ConnectionAbortedError):
                            break
                        if not data:
                            break
                        buffer.extend(data)
                        for kind, payload in extract(buffer):
                            if self.mode == 'silent':
                                continue
                            response = b''
                            if kind == 1 and not payload:
                                response = pack(2)
                            elif kind == 0x1001 and len(payload) == 12:
                                self.commands += 1
                                response = pack(0x2001, payload)
                                if self.mode == 'faults' and not injected:
                                    broken = bytearray(response)
                                    broken[-1] ^= 0x80
                                    response = bytes(broken) + MAGIC + struct.pack('<HI', 9, 0xFFFFFFFF) + response
                                    injected = True
                            if response:
                                try:
                                    if self.mode == 'faults':
                                        # Force fragmentation; client must also parse coalesced frames above.
                                        conn.sendall(response[:3])
                                        time.sleep(0.002)
                                        conn.sendall(response[3:])
                                    else:
                                        conn.sendall(response)
                                except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
                                    break
        except Exception as exc:
            self.error = repr(exc)

    def stop(self):
        self.stop_event.set()
        self.thread.join(timeout=2)
        self.listener.close()
        if self.thread.is_alive():
            raise RuntimeError('mock server did not stop')
        if self.error:
            raise RuntimeError(self.error)

if __name__ == '__main__':
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=8888)
    ap.add_argument('--mode', choices=['normal', 'faults', 'silent', 'reconnect'], default='normal')
    args = ap.parse_args()
    server = MockRobot(args.mode, args.port).start()
    print(f'Mock listening 127.0.0.1:{server.port}; Ctrl+C to stop', flush=True)
    try:
        while True:
            time.sleep(0.5)
    except KeyboardInterrupt:
        server.stop()
