# UDP Camera Streaming (Server + Client)

A minimal live audio/video streaming pair written in C. The **server** captures
video (V4L2) and audio (ALSA) from the host machine using `ffmpeg`, encodes it
to an MPEG-TS stream, and pushes raw packets over UDP. The **client** listens
for that UDP stream and pipes it into `ffplay` for real-time playback.

## Files

| File | Role |
|---|---|
| `camera_server.c` | Captures webcam + mic via `ffmpeg`, streams encoded video over UDP |
| `video_client.c` | Connects to the server, receives the UDP stream, plays it via `ffplay` |

---

## How it works

### `camera_server.c`
1. Opens a UDP socket and binds it to port **9999** on all interfaces.
2. Blocks on `recvfrom()` waiting for *any* incoming packet — this is used
   purely to learn the client's address (a `"hello"` packet sent by the client
   triggers this).
3. Once a client is "connected", it spawns:
   ```
   ffmpeg -f v4l2 -i /dev/video0 -f alsa -i default \
          -vf scale=640:480 -r 30 \
          -c:v mpeg2video -b:v 2M -maxrate 2.5M -bufsize 1M \
          -c:a mp2 -b:a 192k -ar 44100 \
          -f mpegts -fflags nobuffer+fastseek -
   ```
   via `popen()`, capturing `/dev/video0` and the default ALSA mic, encoding
   to MPEG-2 video / MP2 audio inside an MPEG-TS container, written to stdout.
4. Continuously reads chunks from the `ffmpeg` pipe (`fread`) and forwards
   each chunk to the client with `sendto()`.
5. Runs forever (`while(1)`) — there's no clean shutdown path.

### `video_client.c`
1. Opens a UDP socket and sends a `"hello"` packet to the server
   (`SERVER_IP:PORT`) to register itself.
2. Spawns `ffplay` via `popen()` in write mode:
   ```
   ffplay -fflags nobuffer -flags low_delay -framedrop -probesize 32 \
          -sync video -i - -window_title "HD Live Stream" -loglevel quiet
   ```
3. Loops on `recvfrom()`, writing every received UDP packet straight into
   `ffplay`'s stdin and flushing immediately for low latency.

---

## Requirements

- Linux with `gcc`
- `ffmpeg` and `ffplay` installed and on `PATH`
- **Server side:** a V4L2-compatible webcam at `/dev/video0` and an ALSA
  capture device (`default`)
- **Client side:** a display (X11/Wayland) for the `ffplay` window
- Network connectivity between client and server (UDP, port `9999` open)

Install ffmpeg tools (Debian/Ubuntu):
```bash
sudo apt install ffmpeg
```

---

## Build

```bash
gcc -o camera_server camera_server.c
gcc -o video_client video_client.c
```

No extra linker flags are needed — the code only uses standard POSIX sockets
and `libc`.

---

## Run

**On the machine with the webcam (server):**
```bash
./camera_server
```

**On the viewing machine (client):**
```bash
./video_client
```

The client is hardcoded to connect to `10.61.1.118:9999` — edit
`SERVER_IP` in `video_client.c` (and rebuild) to point at your actual server's
IP address before running.

---

## Configuration (streaming)

| Setting | Location | Default |
|---|---|---|
| Server port | `PORT` in both files | `9999` |
| Server IP (client-side) | `SERVER_IP` in `video_client.c` | `10.61.1.118` |
| Video device | hardcoded in `camera_server.c` ffmpeg command | `/dev/video0` |
| Audio device | hardcoded in `camera_server.c` ffmpeg command | `default` (ALSA) |
| Resolution / framerate | ffmpeg `-vf scale` / `-r` flags | `640x480 @ 30fps` |
| Video bitrate | ffmpeg `-b:v` / `-maxrate` | `2M` / `2.5M` |
| Audio bitrate | ffmpeg `-b:a` | `192k` |

---

## Known limitations

- **Single client only** — the server captures the client's address once and
  streams to that address forever; it doesn't support multiple simultaneous
  viewers or reconnects from a different address.
- **No error checking** on `socket()`, `bind()`, or `sendto()`/`recvfrom()`
  return values — failures will silently misbehave rather than exit cleanly.
- **UDP is unreliable** — no retransmission, ordering, or congestion control.
  Expect packet loss/glitches on lossy or congested networks, especially at
  ~2 Mbps+ video bitrate.
- **No authentication/encryption** — anyone who can send a UDP packet to port
  9999 can register as "the client" and start receiving the stream.
- **Hardcoded paths/IPs** (`/dev/video0`, `default`, `10.61.1.118`) — not
  portable without editing and recompiling.
- **Busy loop with `usleep(200)`** in the server's send loop rather than
  event-driven I/O; fine for a small utility, not ideal for CPU efficiency.
- Neither program has a clean exit path (`Ctrl+C` / `SIGINT` kills the
  process; `pclose()`/`close()` are unreachable given the infinite loops).
