from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json


HOST = "127.0.0.1"
PORT = 8765


def proposal_for(payload):
    return {
        "status": "ok",
        "geometries": [],
        "measurements": [],
        "note": "Demo stub: nessuna analisi immagine ancora implementata.",
        "received": {
            "cameraId": payload.get("cameraId", ""),
            "recipeId": payload.get("recipeId", ""),
            "sampleImage": payload.get("sampleImage", ""),
        },
    }


class AssistantHandler(BaseHTTPRequestHandler):
    server_version = "AutoGeometryAssistantDemo/0.1"

    def do_GET(self):
        if self.path not in ("/", "/health"):
            self.send_json(404, {"status": "not_found"})
            return

        self.send_html(
            200,
            """<!doctype html>
<html lang="it">
<head>
  <meta charset="utf-8">
  <title>Auto Geometry Assistant</title>
  <style>
    body { font-family: Segoe UI, Arial, sans-serif; margin: 40px; background: #111318; color: #f2f4f8; }
    code, pre { background: #20242d; border-radius: 6px; padding: 2px 6px; }
    pre { padding: 14px; overflow: auto; }
    .ok { color: #56d364; font-weight: 700; }
  </style>
</head>
<body>
  <h1>Auto Geometry Assistant</h1>
  <p class="ok">Servizio demo attivo.</p>
  <p>Questa e' una prova separata da VisionSystemCPP. Non scrive ricette e non modifica configurazioni.</p>
  <p>Endpoint disponibile:</p>
  <pre>POST /propose-geometries</pre>
  <p>Payload esempio:</p>
  <pre>{
  "cameraId": "CAM01",
  "recipeId": "demo",
  "sampleImage": "sample.png"
}</pre>
</body>
</html>""",
        )

    def do_POST(self):
        if self.path != "/propose-geometries":
            self.send_json(404, {"status": "not_found"})
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            raw_body = self.rfile.read(length) if length > 0 else b"{}"
            payload = json.loads(raw_body.decode("utf-8"))
        except Exception as exc:
            self.send_json(400, {"status": "bad_request", "error": str(exc)})
            return

        self.send_json(200, proposal_for(payload))

    def log_message(self, format, *args):
        return

    def send_html(self, status_code, body):
        data = body.encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def send_json(self, status_code, payload):
        data = json.dumps(payload, indent=2).encode("utf-8")
        self.send_response(status_code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


def main():
    server = ThreadingHTTPServer((HOST, PORT), AssistantHandler)
    print(f"Auto Geometry Assistant demo: http://{HOST}:{PORT}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
