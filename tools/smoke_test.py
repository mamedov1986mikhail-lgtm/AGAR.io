#!/usr/bin/env python3
"""Сквозная проверка сервера реальными TCP/UDP-подключениями.

Запуск:  python3 tools/smoke_test.py build/agar_server
"""
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time

TCP, UDP = 17777, 17778
FULL_MSG = "Сервер полон. Ожидайте своей очереди на вход."
failures = 0


def check(cond, what):
    global failures
    print(("  ok   " if cond else "  FAIL ") + what)
    if not cond:
        failures += 1


class Client:
    def __init__(self):
        self.tcp = socket.create_connection(("127.0.0.1", TCP), timeout=3)
        self.buf = b""
        self.udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.udp.settimeout(1.0)
        self.id = None
        self.token = None

    def send(self, line):
        self.tcp.sendall((line + "\n").encode("utf-8"))

    def readline(self, timeout=3.0):
        self.tcp.settimeout(timeout)
        while b"\n" not in self.buf:
            chunk = self.tcp.recv(4096)
            if not chunk:
                return None
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return line.decode("utf-8")

    def register(self, name, skin=0):
        self.send(f"REG {name} {skin}")
        resp = self.readline()
        if resp and resp.startswith("OK "):
            _, pid, token, _udp, _w, _h = resp.split()
            self.id, self.token = int(pid), int(token)
        return resp

    def send_input(self, x, y):
        self.udp.sendto(struct.pack("<BBIff", 1, self.id, self.token, x, y), ("127.0.0.1", UDP))

    def recv_state(self):
        data, _ = self.udp.recvfrom(4096)
        assert data[0] == 2
        tick, your, np_, nf = struct.unpack_from("<IBBH", data, 1)
        pos = 9
        players = []
        for _ in range(np_):
            pid, skin, x, y, weight, nlen = struct.unpack_from("<BBffIB", data, pos)
            pos += 15
            name = data[pos:pos + nlen].decode()
            pos += nlen
            players.append((pid, name, x, y, weight))
        foods = []
        for _ in range(nf):
            fx, fy, ft = struct.unpack_from("<HHB", data, pos)
            pos += 5
            foods.append((fx, fy, ft))
        return tick, your, players, foods, len(data)

    def close(self):
        self.tcp.close()
        self.udp.close()


def run_server(exe, *extra):
    rec = os.path.join(tempfile.mkdtemp(), "records.txt")
    p = subprocess.Popen([exe, "--tcp", str(TCP), "--udp", str(UDP), "--records", rec, *extra],
                         stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    time.sleep(0.5)
    return p, rec


def main(exe):
    print("== регистрация, UDP-состояние, уникальность имён")
    srv, rec = run_server(exe)
    try:
        a = Client()
        resp = a.register("Alice", 2)
        check(resp is not None and resp.startswith("OK "), f"регистрация: {resp}")
        a.send_input(2000, 2000)
        got = None
        for _ in range(10):
            a.send_input(2000, 2000)
            try:
                got = a.recv_state()
                break
            except socket.timeout:
                pass
        check(got is not None, "получен пакет состояния по UDP")
        if got:
            tick, your, players, foods, size = got
            check(your == a.id, "в пакете верный id игрока")
            check(any(p[1] == "Alice" and p[4] == 1 for p in players), "в пакете есть Alice с весом 1")
            check(size < 1400, f"размер датаграммы {size} < 1400")

        b = Client()
        check(b.register("alice").startswith("ERR TAKEN"), "повтор имени (без учёта регистра) отклонён")
        check(b.register("имя").startswith("ERR BADNAME"), "кириллица в имени отклонена")
        check(b.register("x y").startswith("ERR BADNAME"), "пробел в имени отклонён")
        check(b.register("Bob", 99).startswith("ERR BADSKIN"), "неверный скин отклонён")
        b.close()

        print("== ограничение в 16 игроков и очередь")
        others = []
        for i in range(15):
            c = Client()
            r = c.register(f"P{i}")
            check(r.startswith("OK "), f"игрок {i + 2} принят") if not r.startswith("OK ") else None
            others.append(c)
        ids = {a.id} | {c.id for c in others}
        check(len(ids) == 16, "16 игроков имеют разные id")
        q = Client()
        resp = q.register("Queued")
        check(resp == "FULL " + FULL_MSG, f"17-й получает сообщение: {resp}")

        a.send("LEAVE")
        bye = a.readline()
        check(bye is not None and bye.startswith("BYE "), f"выход подтверждён: {bye}")
        promoted = q.readline()
        check(promoted is not None and promoted.startswith("OK "), "игрок из очереди принят после выхода")

        print("== рекорды")
        a.send("RECORDS")
        head = a.readline()
        check(head == "RECORDS 1", f"в таблице одна запись: {head}")
        row = a.readline()
        check(row is not None and row.startswith("R Alice 1 "), f"запись Alice: {row}")
        check(os.path.exists(rec) and "Alice" in open(rec).read(), "рекорд сохранён в файл")

        print("== обрыв TCP освобождает место")
        others[0].close()
        time.sleep(0.3)
        n = Client()
        r = n.register("Newbie")
        check(r.startswith("OK "), f"после обрыва место свободно: {r}")
    finally:
        srv.terminate()
        srv.wait(timeout=5)

    print("\nПровалов:", failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "build/agar_server"))
