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
        s.connect((PROXY_HOST, PROXY_PORT))
        for _ in range(n_messages):
            t0 = time.perf_counter()
            s.sendall(MESSAGE)
            s.recv(len(MESSAGE))
            latencies.append((time.perf_counter() - t0) * 1_000_000)  # microseconds
        s.close()
    except Exception as e:
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
        print(f"  [ERROR] no successful responses (errors={ERRORS})")
        return

    RESPONSES.sort()
    p50  = RESPONSES[int(total * 0.50)]
    p95  = RESPONSES[int(total * 0.95)]
    p99  = RESPONSES[int(total * 0.99)]
    mean = statistics.mean(RESPONSES)
    rps  = total / elapsed

    print(f"  clients={n_clients:<4}  messages/client={n_messages:<4}  "
          f"total={total:<6}  rps={rps:>8.0f}  "
          f"mean={mean:>7.1f}μs  p50={p50:>7.1f}μs  "
          f"p95={p95:>7.1f}μs  p99={p99:>7.1f}μs  "
          f"errors={ERRORS}")

print("WireFlow benchmark")
print("=" * 90)
for clients in [1, 10, 50, 100]:
    benchmark(clients, 100)
