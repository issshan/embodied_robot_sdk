import json
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from simulator import Simulator, HOME

class SimulatorTests(unittest.TestCase):
    def setUp(self):
        self.sim=Simulator()
        self.peer=('127.0.0.1',12345)
    def send(self,seq=1,enabled=False,target=None,session='one',now=1):
        return self.sim.handle(json.dumps(dict(version=1,session=session,seq=seq,enabled=enabled,target=target or HOME)).encode(),self.peer,now)
    def test_initial_enabled_rejected(self):
        self.assertIsNone(self.send(enabled=True))
    def test_handshake_and_movement(self):
        self.assertIsNotNone(self.send())
        p=json.loads(self.send(seq=2,enabled=True,target=dict(x=10,y=0,z=.2),now=1.1))
        self.assertAlmostEqual(p['target']['x'],.38)
        self.assertEqual(p['ack'],2)
    def test_out_of_order(self):
        self.send(seq=3)
        self.assertIsNone(self.send(seq=2,now=1.1))
        self.assertIsNone(self.send(seq=3,now=1.1))
    def test_session_takeover_blocked(self):
        self.send()
        self.assertIsNone(self.send(session='two',now=1.1))
        self.assertIsNotNone(self.send(session='two',now=1.4))
    def test_watchdog(self):
        self.send();self.send(seq=2,enabled=True,now=1.1)
        self.sim.watchdog(1.5)
        self.assertFalse(self.sim.enabled)
    def test_bad_packets(self):
        for data in (b'{}',b'[]',b'x',b'x'*2049):
            self.assertIsNone(self.sim.handle(data,self.peer,1))
        self.assertIsNone(self.send(seq=True))
        self.assertIsNone(self.send(target=dict(x=float('nan'),y=0,z=0)))
        self.assertIsNone(self.send(target=dict(x=True,y=0,z=0)))
    def test_disabled_holds(self):
        self.send(target=dict(x=.5,y=.25,z=.3))
        self.assertEqual(self.sim.target,HOME)
    def test_peer_lock(self):
        self.send()
        p=dict(version=1,session='one',seq=2,enabled=False,target=HOME)
        self.assertIsNone(self.sim.handle(json.dumps(p).encode(),('127.0.0.1',54321),1.1))
if __name__=='__main__':
    unittest.main()
