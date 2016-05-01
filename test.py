#!/usr/bin/env python

from __future__ import print_function

import unittest
import subprocess
import contextlib
import requests
import random
import threading


class FathomTest(unittest.TestCase):

    def test_syzygy_required(self):
        self.assertEqual(subprocess.call(["./fathom", "--syzygy", "gaviota"]), 78)

    @contextlib.contextmanager
    def fathom(self, *args, **kwargs):
        port = random.randint(30000, 39999)

        process = subprocess.Popen(
            ["./fathom", "--port", str(port),
             "--syzygy", kwargs.get("syzygy", "syzygy"),
             "--gaviota", kwargs.get("gaviota", "gaviota")] + list(args),
            stdout=subprocess.PIPE,
            bufsize=1,
            universal_newlines=True)

        listening = threading.Event()

        def stdout_reader():
            for line in iter(process.stdout.readline, ""):
                print(line.rstrip())
                if line.startswith("listening"):
                    listening.set()

            listening.set()

        reader_thread = threading.Thread(target=stdout_reader)
        reader_thread.start()
        listening.wait()

        yield "http://127.0.0.1:%d/tablebase" % port

        process.terminate()
        reader_thread.join()

    def test_no_cors(self):
        with self.fathom() as endpoint:
            res = requests.get(endpoint, {"fen": "4k3/8/8/8/8/8/8/1R2K3 w - - 0 1"})
            self.assertFalse("Access-Control-Allow-Origin" in res.headers)

    def test_cors(self):
        with self.fathom("--cors") as endpoint:
            res = requests.get(endpoint, {"fen": "4k3/8/8/8/8/8/8/1R2K3 w - - 0 1"})
            self.assertEqual(res.headers["Access-Control-Allow-Origin"], "*")

    def test_kqk(self):
        with self.fathom() as endpoint:
            data = requests.get(endpoint, {"fen": "K7/8/8/4k3/8/2Q5/8/8 b - - 0 1"}).json()
            self.assertEqual(data["dtz"], -18)
            self.assertEqual(data["moves"][0]["dtm"], 17)
            self.assertEqual(data["moves"][0]["san"], "Ke4")
            self.assertEqual(data["moves"][0]["uci"], "e5e4")
            self.assertFalse(data["moves"][0]["checkmate"])
            self.assertFalse(data["moves"][0]["stalemate"])
            self.assertFalse(data["moves"][0]["zeroing"])
            self.assertEqual(data["wdl"], -2)
            self.assertEqual(data["real_wdl"], -2)


if __name__ == "__main__":
    unittest.main()
