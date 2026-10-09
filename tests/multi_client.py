import socket
import subprocess
import sys
import time

with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.bind(('127.0.0.1', 0))
    port = str(sock.getsockname()[1])

processes = []
try:
    processes.append(subprocess.Popen([sys.argv[1], 'server', port], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
    time.sleep(0.05)
    for _ in range(2):
        processes.append(subprocess.Popen([sys.argv[1], 'client', port], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True))
    for process in processes:
        output, error = process.communicate(timeout=10)
        print(output, end='')
        if process.returncode:
            raise RuntimeError(error)
finally:
    for process in processes:
        if process.poll() is None:
            process.kill()
            process.wait()
