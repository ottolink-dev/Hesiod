import socket
import struct
import threading
import zlib
import numpy as np
import bpy

from .constants import HOST
from .mesh import update_mesh
from . import state


# --- Network streaming


def recv_all(sock, size):
    data = b""
    while len(data) < size:
        chunk = sock.recv(size - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def stream_loop(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        sock.connect((HOST, port))
        state.connected = True
        print(f"[Hesiod] Connected on port {port}")

        while True:
            # --- Header

            header = recv_all(sock, 20)
            if not header:
                break

            tid, width, height, hm_size, has_texture = struct.unpack(
                "IIIII", header)

            print(f"[Hesiod] Received terrain id={tid} ({width}x{height})")

            # --- Heightmap

            compressed_hm = recv_all(sock, hm_size)
            if compressed_hm is None:
                break

            raw_hm = zlib.decompress(compressed_hm)
            heightmap = np.frombuffer(raw_hm, dtype=np.float32).reshape(
                (height, width))

            rgba = None

            # --- Texture

            if has_texture:
                tex_size_bytes = recv_all(sock, 4)
                if tex_size_bytes is None:
                    break

                tex_size = struct.unpack("I", tex_size_bytes)[0]

                compressed_rgba = recv_all(sock, tex_size)
                if compressed_rgba is None:
                    break

                raw_rgba = zlib.decompress(compressed_rgba)
                rgba = np.frombuffer(raw_rgba, dtype=np.float32).reshape(
                    (height, width, 4))

            bpy.app.timers.register(
                lambda hm=heightmap, c=rgba, t=tid: update_mesh(t, hm, c))

    except Exception as e:
        print("[Hesiod] Socket error:", e)

    finally:
        state.connected = False
        sock.close()
        print("[Hesiod] Disconnected")


def start_stream(port):
    if state.thread and state.thread.is_alive():
        print("[Hesiod] Already running")
        return

    state.thread = threading.Thread(target=stream_loop, args=(port, ), daemon=True)
    state.thread.start()
    print(f"[Hesiod] Stream started on port {port}")
