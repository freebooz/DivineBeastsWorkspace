"""UE HTTP重定向负例夹具：仅回环双端口，无真实凭据，不打印请求头或请求体。

必须由操作者显式启动；它不是UE宿主，也不代替UE真实请求验收。
--output 指向工作空间Saved下的输出JSON；端口0让系统分配，避免占用他人端口。
"""

import argparse
import json
import re
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


class RedirectState:
    def __init__(self):
        self.lock = threading.Lock()
        self.counts = {}
        self.origin_port = 0
        self.target_port = 0


def handler_for(state):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *_):
            pass  # 不输出Authorization、body或URL中的运行身份。

        def respond(self, status, body=b"", location=None):
            self.send_response(status)
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Content-Type", "text/plain")
            if location:
                self.send_header("Location", location)
            self.end_headers()
            self.wfile.write(body)

        def do_POST(self):
            self.do_GET()

        def do_GET(self):
            # 只接受有界测试报文；既不保存也不输出载荷。
            try:
                size = int(self.headers.get("Content-Length", "0"))
            except ValueError:
                self.respond(400)
                return
            if size < 0 or size > 4096:
                self.respond(413)
                return
            self.rfile.read(size)
            match = re.fullmatch(r"/redirect/(301|302|303|307|308)/(same|cross)/([0-9A-Fa-f]{32})", self.path)
            if match:
                code, scope, run_id = match.groups()
                port = state.origin_port if scope == "same" else state.target_port
                self.respond(int(code), b"redirect-rejected-test", f"http://127.0.0.1:{port}/sink/{run_id}")
                return
            match = re.fullmatch(r"/(sink|sink-count)/([0-9A-Fa-f]{32})", self.path)
            if match:
                route, run_id = match.groups()
                with state.lock:
                    if route == "sink":
                        state.counts[run_id] = state.counts.get(run_id, 0) + 1
                    count = state.counts.get(run_id, 0)
                self.respond(200, str(count).encode("ascii"))
                return
            self.respond(404)
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    # 不覆盖已有运行证据；操作者选择唯一Saved路径。
    args.output.parent.mkdir(parents=True, exist_ok=True)
    state = RedirectState()
    origin = ThreadingHTTPServer(("127.0.0.1", 0), handler_for(state))
    target = ThreadingHTTPServer(("127.0.0.1", 0), handler_for(state))
    state.origin_port, state.target_port = origin.server_port, target.server_port
    with args.output.open("x", encoding="utf-8") as output:
        json.dump({"origin": f"http://127.0.0.1:{state.origin_port}", "target": f"http://127.0.0.1:{state.target_port}"}, output)
    worker = threading.Thread(target=target.serve_forever, daemon=True)
    worker.start()
    try:
        origin.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        target.shutdown()
        origin.server_close()
        target.server_close()


if __name__ == "__main__":
    main()
