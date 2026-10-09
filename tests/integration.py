import heapq
import socket
import struct
import subprocess
import sys
import threading
import time

HEADER = 38


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]


class Proxy:
    def __init__(self, server_port, lossy):
        self.server = ('127.0.0.1', server_port)
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', 0))
        self.sock.settimeout(0.01)
        self.port = self.sock.getsockname()[1]
        self.client = None
        self.lossy = lossy
        self.running = True
        self.dropped_welcome = False
        self.forged_input = False
        self.enemy = None
        self.delayed = []
        self.counter = 0
        self.thread = threading.Thread(target=self.run)
        self.thread.start()

    def run(self):
        while self.running:
            now = time.monotonic()
            while self.delayed and self.delayed[0][0] <= now:
                _, _, data, destination = heapq.heappop(self.delayed)
                self.sock.sendto(data, destination)
            try:
                data, source = self.sock.recvfrom(2048)
            except socket.timeout:
                continue
            server = source == self.server
            if not server:
                self.client = source
            destination = self.client if server else self.server
            if destination is None:
                continue
            frames = []
            offset = HEADER
            while offset + 3 <= len(data):
                op, size = struct.unpack_from('<BH', data, offset)
                frames.append((op, offset + 3, size))
                offset += 3 + size
            if "--trace" in sys.argv:
                print("server" if server else "client", frames, data[:HEADER].hex(), flush=True)
            for op, offset, size in frames:
                if server and op == 5 and size == 24:
                    entity, revision, owner = struct.unpack_from('<III', data, offset)
                    if owner == 0:
                        self.enemy = entity
                if not server and op == 8 and self.enemy and not self.forged_input:
                    # Same legitimate endpoint/session, unauthorized entity.
                    packet = bytearray(data)
                    struct.pack_into('<I', packet, offset, self.enemy)
                    data = bytes(packet)
                    self.forged_input = True
            if self.lossy and server and any(op == 1 for op, _, _ in frames) and not self.dropped_welcome:
                self.dropped_welcome = True
                continue
            self.counter += 1
            if self.lossy and self.counter % 7 == 0:
                continue
            if self.lossy and self.counter % 5 == 0:
                heapq.heappush(self.delayed, (now + 0.09, self.counter, data, destination))
            else:
                self.sock.sendto(data, destination)
                if self.lossy and self.counter % 11 == 0:
                    self.sock.sendto(data, destination)

    def close(self):
        self.running = False
        self.thread.join()
        self.sock.close()


def run(binary, lossy=False, incompatible=False):
    port = free_port()
    server = subprocess.Popen([binary, 'server', str(port)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    proxy = Proxy(port, lossy)
    try:
        time.sleep(0.05)
        args = [binary, 'client', str(proxy.port)]
        if incompatible:
            args.append('incompatible')
        client = subprocess.run(args, capture_output=True, text=True, timeout=12)
        if incompatible:
            print(client.stdout, end='')
            if client.returncode:
                raise RuntimeError(client.stderr)
            return
        output, error = server.communicate(timeout=12)
        print(client.stdout, end='')
        print(output, end='')
        if client.returncode or server.returncode:
            raise RuntimeError(client.stderr + error)
        if not proxy.forged_input:
            raise RuntimeError('Ownership attack was not exercised')
        if lossy and not proxy.dropped_welcome:
            raise RuntimeError('Handshake retry was not exercised')
    finally:
        proxy.close()
        if server.poll() is None:
            server.kill()
            server.wait()


if __name__ == '__main__':
    run(sys.argv[1], '--lossy' in sys.argv, '--incompatible' in sys.argv)
