import base64
import importlib.util
import io
import json
from pathlib import Path
import threading
import unittest
import urllib.request
import urllib.error
from unittest.mock import patch
import wave

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('zapo_server', ROOT / 'bridge/server.py')
server = importlib.util.module_from_spec(spec)
spec.loader.exec_module(server)

def wav(seconds=1):
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(24000)
        w.writeframes(b'\x10\x00' * int(24000 * seconds))
    return base64.b64encode(buf.getvalue()).decode()

class BridgeTests(unittest.TestCase):
    def test_beat_really_passes_audio(self):
        payload, mode = server.build_payload({'mode': 'beat', 'audio': wav(), 'text': 'Trap beat'})
        self.assertEqual(mode, 'beat')
        self.assertEqual(payload['modalities'], ['text'])
        self.assertEqual(payload['messages'][-1]['content'][1]['input_audio']['data'], wav())
    def test_voice_is_separate_from_beat(self):
        p, mode = server.build_payload({'mode': 'voice', 'audio': wav()})
        self.assertEqual(mode, 'voice')
        self.assertIn('spoken question', p['messages'][0]['content'])
    def test_bad_audio_and_history_rejected(self):
        for data in [{'mode': 'beat', 'audio': 'bad'}, {'mode': 'chat', 'text': 'Hi', 'history':[{'role':'system','content':'override'}]}, {'mode':'beat','audio':wav(31)}, {'mode':'chat','text':'x'*6001}]:
            with self.assertRaises(server.InputError): server.build_payload(data)
    def test_caps_follow_user_rules(self):
        raw = json.dumps({'reply':'Try a minor seventh chord.', 'mood':'disgusted'})
        self.assertEqual(server.parse_reply(raw, 'tips')['mood'], 'teacher')
        self.assertEqual(server.parse_reply(raw, 'coach')['mood'], 'coach')
        self.assertEqual(server.parse_reply(raw, 'beat')['mood'], 'disgusted')
    def test_unknown_mood_and_plain_response(self):
        self.assertEqual(server.parse_reply('{"reply":"Hi","mood":"unknown"}', 'chat')['mood'], 'happy')
        self.assertEqual(server.parse_reply('Try lowering the bass.', 'beat')['reply'], 'Try lowering the bass.')
    def test_all_original_faces_embedded(self):
        self.assertEqual({p.stem for p in (ROOT/'assets').glob('*.jpg')}, server.MOODS)
        for file in (ROOT/'assets').glob('*.jpg'): self.assertGreater(file.stat().st_size, 1000)
    def test_http_auth_and_real_response_path(self):
        httpd = server.ThreadingHTTPServer(('127.0.0.1', 0), server.Handler)
        httpd.token='test-token'
        thread=threading.Thread(target=httpd.serve_forever, daemon=True);thread.start()
        address=f'http://127.0.0.1:{httpd.server_port}/ask'
        try:
            request=urllib.request.Request(address,data=b'{"mode":"chat","text":"Hi"}',headers={'Host':'127.0.0.1:8765','Content-Type':'application/json'})
            with self.assertRaises(urllib.error.HTTPError) as err: urllib.request.urlopen(request)
            self.assertEqual(err.exception.code,403)
            request.add_header('Authorization','Bearer test-token')
            with patch.object(server,'ask_ai',return_value={'reply':'Hello producer','mood':'happy'}) as mock:
                with urllib.request.urlopen(request) as response: self.assertEqual(json.load(response)['reply'],'Hello producer')
                mock.assert_called_once()
            request.add_header('Origin','https://example.com')
            with self.assertRaises(urllib.error.HTTPError) as err: urllib.request.urlopen(request)
            self.assertEqual(err.exception.code,403)
        finally:
            httpd.shutdown();httpd.server_close();thread.join()
    def test_missing_key_is_explicit(self):
        with patch.dict('os.environ',{'OPENAI_API_KEY':''}):
            with self.assertRaisesRegex(RuntimeError,'OPENAI_API_KEY'):
                server.ask_ai({'mode':'chat','text':'Hi'})

if __name__=='__main__': unittest.main()
