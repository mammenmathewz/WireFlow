import socket
import threading
import time
import statistics

PROXY_HOST = "127.0.0.1"
PROXY_PORT = 8080
MESSAGE    = b"ping"
RESPONSES  = []
ERRORS     = 0
LOCK       = threading.Lock()

def run_client(n_messages):
    global ERRORS
    latencies = []
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((PROXY_HOST, PROXY_PORT))
        for _ in range(n_messages):
            t0 = time.perf_counter()
            s.sendall(MESSAGE)
            s.recv(len(MESSAGE))
            latencies.append((time.perf_counter() - t0) * 1_000_000)
        s.close()
    except Exception:
        with LOCK:
            ERRORS += 1
    with LOCK:
        RESPONSES.extend(latencies)

def benchmark(n_clients, n_messages):
    global RESPONSES, ERRORS
    RESPONSES = []
    ERRORS    = 0

    threads = [threading.Thread(target=run_client, args=(n_messages,))
               for _ in range(n_clients)]

    t_start = time.perf_counter()
    for t in threads: t.start()
    for t in threads: t.join()
    elapsed = time.perf_counter() - t_start

    total = len(RESPONSES)
    if total == 0:
        print(f"  clients={n_clients:<5}  [DEAD — all {ERRORS} connections failed]")
        return

    RESPONSES.sort()
    p50  = RESPONSES[int(total * 0.50)]
    p99  = RESPONSES[int(total * 0.99)]
    rps  = total / elapsed
    error_pct = (ERRORS / (total + ERRORS)) * 100

    status = "OK" if ERRORS == 0 else f"DEGRADED ({error_pct:.1f}% errors)"
    print(f"  clients={n_clients:<5}  rps={rps:>8.0f}  "
          f"p50={p50:>8.1f}μs  p99={p99:>9.1f}μs  "
          f"errors={ERRORS:<5}  [{status}]")

print("WireFlow break-point test")
print("=" * 90)
for clients in [100, 250, 500, 750, 1000, 2000, 5000]:
    benchmark(clients, 50)
    time.sleep(1)  # let proxy recover between runs