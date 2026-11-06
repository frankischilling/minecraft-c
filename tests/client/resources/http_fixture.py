"""Authored loopback HTTP input for native resource controls.

This is a real socket peer, not a production URL/provider replacement.
The CTest driver starts and stops it on an ephemeral loopback port.
"""
import argparse
import http.server
import json
import threading

XML = b"<ListBucketResult><Contents><Key><![CDATA[ignored/data.bin]]>unused</Key><Size><!--5-->999</Size></Contents></ListBucketResult>"
DATA = bytes((1, 2, 3, 4, 5))
LARGE = bytes((index * 17 + 3) % 256 for index in range(65539))

class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def do_GET(self):
        if self.path == "/":
            status, body, content = 200, XML, "application/xml"
        elif self.path in ("/ignored/data.bin", "/ignored/cancel.bin"):
            status, body, content = 200, DATA, "application/octet-stream"
        elif self.path == "/ignored/large.bin":
            status, body, content = 200, LARGE, "application/octet-stream"
        elif self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "/ignored/data.bin")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        else:
            status, body, content = 404, b"not found", "text/plain"
        self.send_response(status)
        self.send_header("Content-Type", content)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()
    def do_POST(self):
        if self.path != "/stop":
            self.send_error(404)
            return
        self.send_response(204)
        self.send_header("Content-Length", "0")
        self.end_headers()
        threading.Thread(target=self.server.shutdown, daemon=True).start()
    def log_message(self, message, *args):
        print(json.dumps({"actual_request": message % args}), flush=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=int, default=0)
    args = parser.parse_args()
    with http.server.ThreadingHTTPServer(("127.0.0.1", args.port), Handler) as server:
        print(json.dumps({"listening": server.server_address, "fixture": "authored XML/5-byte payload"}), flush=True)
        server.serve_forever(poll_interval=0.1)
