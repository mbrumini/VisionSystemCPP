$ErrorActionPreference = "Stop"

$pipe = [System.IO.Pipes.NamedPipeClientStream]::new(
  ".",
  "VisionSystemSimulator",
  [System.IO.Pipes.PipeDirection]::InOut,
  [System.IO.Pipes.PipeOptions]::None)
$pipe.Connect(5000)

$reader = [System.IO.StreamReader]::new($pipe, [System.Text.Encoding]::UTF8, $false, 4096, $true)
$writer = [System.IO.StreamWriter]::new($pipe, [System.Text.UTF8Encoding]::new($false), 4096, $true)
$writer.AutoFlush = $true

$hello = $reader.ReadLine()
$writer.WriteLine('{"type":"ping","protocolVersion":1}')
$pong = $reader.ReadLine()

$bmpBase64 = "Qk06AAAAAAAAADYAAAAoAAAAAQAAAAEAAAABABgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA8PDwAA=="
$frame = @{
  type = "frame"
  protocolVersion = 1
  scenarioId = "smoke-test"
  cameraId = "CAM01"
  channel = "CAM01"
  slot = 1
  frameId = 1
  timestamp = [DateTimeOffset]::Now.ToString("o")
  imageFormat = "bmp"
  imageBase64 = $bmpBase64
} | ConvertTo-Json -Compress
$writer.WriteLine($frame)
$accepted = $reader.ReadLine()

$helloObject = $hello | ConvertFrom-Json
$pongObject = $pong | ConvertFrom-Json
$acceptedObject = $accepted | ConvertFrom-Json
if ($helloObject.type -ne "hello" -or
    $pongObject.type -ne "pong" -or
    $acceptedObject.type -ne "frameAccepted" -or
    $acceptedObject.frameId -ne 1 -or
    $acceptedObject.slot -ne 1) {
  throw "Protocollo simulatore non valido"
}

"HELLO $hello"
"PONG $pong"
"FRAME_ACCEPTED $accepted"

$reader.Dispose()
$writer.Dispose()
$pipe.Dispose()
