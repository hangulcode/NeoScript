"""Local HTTP integration checks for a console linked with libcurl."""
import argparse
import http.server
import pathlib
import subprocess
import tempfile
import threading
import time


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/slow":
            time.sleep(0.15)
        self.send_response(404 if self.path == "/missing" else 200)
        self.end_headers()
        try:
            self.wfile.write(b"get-response")
        except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError):
            pass

    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        self.send_response(200)
        self.end_headers()
        self.wfile.write(self.headers.get("X-Test", "").encode() + b":" + body)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("console", type=pathlib.Path)
    args = parser.parse_args()
    console = args.console.resolve()
    root = pathlib.Path(__file__).resolve().parents[1]
    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    base = "http://127.0.0.1:" + str(server.server_port)
    cases = [
        (f'a.get(1000,"{base}/",Done); a.wait();', 'ok && text=="get-response"'),
        (f'a.add_header("X-Test","header"); a.post(1000,"{base}/","payload",Done); a.wait();',
         'ok && text=="header:payload"'),
        (f'a.get(1000,"{base}/missing",Done); a.wait();', 'ok && text=="get-response"'),
        ('a.get(1000,"file:///not-http",Done); a.wait();', '!ok'),
        (f'a.add_header("bad\\nname","x"); a.get(1000,"{base}/",Done); a.wait();', '!ok'),
        (f'a.get(10,"{base}/slow",Done); while(!done) a.wait();', '!ok'),
    ]
    try:
        with tempfile.TemporaryDirectory(prefix="neo-http-") as folder:
            for i, (request, expected) in enumerate(cases):
                script = pathlib.Path(folder) / f"case{i}.ns"
                script.write_text('import system; var passed=false; var done=false; '
                    f'fun Done(var ok,var text){{passed={expected}; done=true;}} '
                    f'var a=system.aysnc_create(); {request} print(passed);', encoding="utf-8")
                result = subprocess.run([str(console), "--file", str(script)], cwd=root / "Samples/console",
                                        capture_output=True, text=True, timeout=10)
                if result.returncode or result.stdout.strip() != "true":
                    raise RuntimeError(f"case {i}: {result.returncode}\n{result.stdout}\n{result.stderr}")
        print(f"HTTP console: {len(cases)} cases passed")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
