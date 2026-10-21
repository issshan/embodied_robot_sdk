"""Loopback UDP simulator; no robot driver, no real actuator commands."""
import argparse
import json
import math
import socket
import time

MIN = (0.15, -0.25, 0.05)
MAX = (0.5, 0.25, 0.3)
HOME = dict(x=0.35, y=0.0, z=0.2)

def valid_vec(v):
    return isinstance(v, dict) and all(type(v.get(k)) in (int, float) and math.isfinite(v[k]) for k in ('x', 'y', 'z'))

class Simulator:
    def __init__(self):
        self.session = None
        self.peer = None
        self.seq = -1
        self.last_seen = -math.inf
        self.target = dict(HOME)
        self.enabled = False

    def handle(self, data, peer, now):
        if len(data) > 2048:
            return None
        try:
            p = json.loads(data)
        except (ValueError, UnicodeError):
            return None
        if not isinstance(p, dict) or p.get('version') != 1 or type(p.get('seq')) is not int or not 0 <= p['seq'] <= 2**53:
            return None
        session = p.get('session')
        if not isinstance(session, str) or not 1 <= len(session) <= 64 or type(p.get('enabled')) is not bool or not valid_vec(p.get('target')):
            return None
        if (session, peer) != (self.session, self.peer):
            if now-self.last_seen < 0.3 or p['enabled']:
                return None
            self.session, self.peer, self.seq = session, peer, -1
        if p['seq'] <= self.seq:
            return None
        self.seq = p['seq']
        dt = min(0.1, max(0.0, now-self.last_seen))
        self.last_seen = now
        self.enabled = p['enabled']
        if self.enabled:
            desired = [max(MIN[i], min(MAX[i], p['target'][k])) for i,k in enumerate(('x','y','z'))]
            delta = [desired[i]-self.target[k] for i,k in enumerate(('x','y','z'))]
            distance = math.sqrt(sum(d*d for d in delta))
            factor = min(1, 0.3*dt/max(distance, 1e-12))
            self.target = {k:self.target[k]+delta[i]*factor for i,k in enumerate(('x','y','z'))}
        return json.dumps(dict(version=1,session=self.session,ack=self.seq,enabled=self.enabled,target=self.target,status='simulated')).encode()

    def watchdog(self, now):
        if now-self.last_seen >= 0.3:
            self.enabled = False

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--host', default='127.0.0.1', help='For Quest LAN tests explicitly choose a private LAN interface; unauthenticated simulator only')
    ap.add_argument('--port', type=int, default=8765)
    args = ap.parse_args()
    sim = Simulator()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.bind((args.host,args.port))
        sock.settimeout(0.05)
        print(f'SIMULATOR ONLY {args.host}:{sock.getsockname()[1]}',flush=True)
        try:
            while True:
                sim.watchdog(time.monotonic())
                try:
                    data,peer=sock.recvfrom(4096)
                except socket.timeout:
                    continue
                reply=sim.handle(data,peer,time.monotonic())
                if reply:
                    sock.sendto(reply,peer)
        except KeyboardInterrupt:
            pass
if __name__ == '__main__':
    main()
