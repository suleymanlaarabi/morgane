#!/usr/bin/env python3
"""End-to-end match scenarios: a real r-type_server and headless r-type_bot clients on localhost.

Usage: match_test.py crowd|crash|restart SERVER BOT
"""
import socket
import subprocess
import sys
import threading
import time

TIMEOUT, CAPACITY = 1, 3  # net::DisconnectReason values
PLAYING, OVER = 1, 2  # game::Phase values


def free_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return probe.getsockname()[1]


class Process:
    """A child process whose stdout lines are collected in the background."""

    def __init__(self, *command):
        self.lines = []
        self.process = subprocess.Popen(command, stdout=subprocess.PIPE, text=True)
        threading.Thread(target=self._read, daemon=True).start()

    def _read(self):
        for line in self.process.stdout:
            self.lines.append(line.strip())

    def has(self, prefix):
        return any(line.startswith(prefix) for line in self.lines)

    def wait_for(self, prefix, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            if self.has(prefix):
                return True
            time.sleep(0.05)
        return False

    def finish(self, seconds):
        try:
            return self.process.wait(seconds)
        except subprocess.TimeoutExpired:
            self.process.kill()
            return None


def fail(message, *processes):
    print("FAIL:", message)
    for index, process in enumerate(processes):
        print(f"--- process {index}:", *process.lines, sep="\n  ")
    sys.exit(1)


def main():
    scenario, server_path, bot_path = sys.argv[1:4]
    port = str(free_port())
    server = Process(server_path, port)
    if not server.wait_for("r-type_server listening", 10):
        fail("server did not start", server)

    def bot(seconds, *flags):
        return Process(bot_path, "127.0.0.1", port, str(seconds), *flags)

    try:
        if scenario == "crowd":
            bots = [bot(12, "--fire") for _ in range(4)]
            for b in bots:
                if not b.wait_for("spawned", 10):
                    fail("a bot did not join", *bots)
            extra = bot(5)
            if not extra.wait_for(f"disconnected {CAPACITY}", 10):
                fail("a fifth player was not refused as the match is full", extra, *bots)
            for b in bots:
                b.finish(30)
            slots = sorted(next(l for l in b.lines if l.startswith("spawned")) for b in bots)
            if slots != [f"spawned slot={i}" for i in range(4)]:
                fail(f"players did not get the four slots: {slots}", *bots)
            if not all(b.has(f"phase {PLAYING}") for b in bots):
                fail("a bot never saw the match playing", *bots)
            totals = {}
            for b in bots:
                for line in b.lines:
                    if line.startswith("done"):
                        for key, value in (field.split("=") for field in line.split()[1:]):
                            totals[key] = totals.get(key, 0) + int(value)
            if not totals.get("destroyed"):
                fail("four firing players never destroyed anything", *bots)
            # Spawn snapshots must carry the values set with Replicated, or every body looks
            # like a player shot on the clients.
            if not totals.get("enemies") or not totals.get("enemy_shots"):
                fail(f"clients never saw an enemy or an enemy shot: {totals}", *bots)
        elif scenario == "crash":
            bots = [bot(20, "--fire") for _ in range(3)]
            for b in bots:
                if not b.wait_for("spawned", 10):
                    fail("a bot did not join", *bots)
            crashed = bots.pop()
            slot = next(l for l in crashed.lines if l.startswith("spawned")).split("=")[1]
            crashed.process.kill()
            for b in bots:
                if not b.wait_for(f"left {slot} {TIMEOUT}", 12):
                    fail(f"the crash of player slot {slot} was not announced", *bots)
            if server.process.poll() is not None:
                fail("the server stopped after a client crash", server)
            for b in bots:
                if b.has("disconnected"):
                    fail("a remaining player was disconnected", *bots)
            replacement = bot(3)
            if not replacement.wait_for(f"spawned slot={slot}", 10):
                fail("the freed slot could not be joined again", replacement)
        elif scenario == "restart":
            # Bots that never fire are worn down by the waves, then vote to play again.
            bots = [bot(110, "--vote") for _ in range(2)]
            deadline = time.monotonic() + 105
            while time.monotonic() < deadline:
                if all(b.has(f"phase {OVER}") for b in bots):
                    break
                time.sleep(0.2)
            else:
                fail("the match never ended", *bots)
            for b in bots:
                over = b.lines.index(next(l for l in b.lines if l.startswith(f"phase {OVER}")))
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline and not any(
                    l.startswith(f"phase {PLAYING}") for l in b.lines[over:]
                ):
                    time.sleep(0.05)
                if not any(l.startswith(f"phase {PLAYING}") for l in b.lines[over:]):
                    fail("the match did not restart after every player voted", *bots)
                if b.lines[-1] != "health 3" and "health 3" not in b.lines[over:]:
                    fail("a player did not respawn with full health", *bots)
        else:
            fail(f"unknown scenario {scenario}")
    finally:
        for process in [server] + list(locals().get("bots", [])):
            if process.process.poll() is None:
                process.process.kill()
    print("PASS:", scenario)


if __name__ == "__main__":
    main()
