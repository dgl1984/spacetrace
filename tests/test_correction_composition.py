"""Check cascade equivalence, input preservation, and the approved direct correction pairs."""
from pathlib import Path
import hashlib
import json
import sys
import tempfile
import unittest

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from compose_corrections import compose
from normalize_correction import read_wav, write_float32_mono
from tool_common import ToolError


class CompositionTests(unittest.TestCase):
    def test_rendered_pair_equals_post_sum_residual(self):
        # Unequal source/ear paths and unequal gains catch a mistaken channel
        # balance adjustment, filter addition, or automatic normalization.
        rng = np.random.default_rng(471)
        source_l, source_r = rng.normal(size=(2, 99))
        base_l = np.array([.3, -.2, .11], dtype=np.float32)
        base_r = np.array([.7, .05, -.1, .02], dtype=np.float32)
        residual = np.array([1.21, -.15, .06], dtype=np.float32)
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            write_float32_mono(d / "residual.wav", 48000, residual)
            merged = []
            for side, base in (("l", base_l), ("r", base_r)):
                p = d / (side + ".wav")
                write_float32_mono(p, 48000, base)
                original = p.read_bytes()
                compose(p, d / "residual.wav", d / (side + "-merged.wav"))
                merged.append(read_wav(d / (side + "-merged.wav"))[1])
                self.assertEqual(p.read_bytes(), original)
            def add(a, b):
                n = max(len(a), len(b))
                return np.pad(a, (0, n-len(a))) + np.pad(b, (0, n-len(b)))
            for ear_l, ear_r in (([1, .1], [.13, -.02]), ([.22, .03], [.8, -.07])):
                a = np.convolve(source_l, ear_l) * .83
                b = np.convolve(source_r, ear_r) * 1.07
                expected = np.convolve(add(np.convolve(a, base_l), np.convolve(b, base_r)), residual)
                actual = add(np.convolve(a, merged[0]), np.convolve(b, merged[1]))
                np.testing.assert_allclose(actual, expected, rtol=3e-6, atol=2e-7)

    def test_invalid_input_does_not_write_output(self):
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            write_float32_mono(d / "base.wav", 48000, [1, .2])
            for sr, samples in ((44100, [1, 0]), (48000, [float('nan'), 0]), (48000, [0, 0])):
                write_float32_mono(d / "extra.wav", sr, samples)
                with self.assertRaises(ToolError):
                    compose(d / "base.wav", d / "extra.wav", d / "out.wav")
                self.assertFalse((d / "out.wav").exists())
            with self.assertRaises(ToolError):
                compose(d / "base.wav", d / "base.wav", d / "base.wav", overwrite=True)

    def test_tail_fade_preserves_onset_and_reaches_zero(self):
        with tempfile.TemporaryDirectory() as td:
            d = Path(td)
            write_float32_mono(d / "base.wav", 1000, [1, .5, .4, .3, .2, .1])
            write_float32_mono(d / "extra.wav", 1000, [1])
            compose(d / "base.wav", d / "extra.wav", d / "out.wav", tail_fade_ms=4)
            actual = read_wav(d / "out.wav")[1]
            # Four-sample half cosine has weights 1, .75, .25, 0.
            np.testing.assert_allclose(actual, [1, .5, .4, .225, .05, 0], atol=2e-8)
            for fade in (-1, float('nan'), float('inf'), 100, .1):
                with self.assertRaises(ToolError):
                    compose(d / "base.wav", d / "extra.wav", d / "bad.wav", tail_fade_ms=fade)
                self.assertFalse((d / "bad.wav").exists())

    def test_approved_direct_pairs_in_plugin_order(self):
        # Freeze the listening-approved release assets by head identity. This
        # catches swapping valid correction files between head packages.
        approved = {'IRCAM_1050': ['5c677b8f5ffe1de4b83872ec6e3b260e7a8a4b44cd26444c0f1a352eb43944d1', 'c8aee734e886ed298fe1879b2bb09d4f9a9a1ab514cbd9ad478b5719c11ee480'], 'MIT_KEMAR_Normal': ['85a4ce47a27b52113fb81308243a0067503fe7edb7795502266738c6c4a81564', '24bb6d3b272c1a01f9b73a55517717a9ff500c4c1d0a3a669c64596bcff038df'], 'KU100_SADIE_D1': ['0884796eae4aa01bba4f2b63b58cc0ef2f78fd1f1e0405b20c9b2bbedcb211e4', '081a0d7777884111443d58a785b94854ccae5c0e3926628039a6a03e60392bb8'], 'KU100_FULL2DEG': ['f073a875d87e314006d804fdfc91369899db857f06a6ce11c14f9a431f8fcecf', '57be05cb7d49a52b9e552390ee1a87fcdbf32340880f271e1f7402d8909d7207'], 'FABIAN_HATO0': ['29c7dae2e00bf49cb720f8b3cc246a3fd1fc37a0f1ab47bffed63d10b0ce03c3', 'e145141883da3b02af076241fabbbc3bb07c932a2357b5348c06fe0f9cc1737c']}
        for folder, digests in approved.items():
            head = ROOT / "Heads" / folder
            manifest = json.loads((head / "manifest.json").read_text(encoding="utf-8"))
            provenance = json.loads((head / "provenance.json").read_text(encoding="utf-8"))
            self.assertNotIn("stereoPairResidualComposition", provenance)
            for side, digest in zip(("Left", "Right"), digests):
                key = "stereoPair" + side + "Correction"
                output = head / manifest[key + "File"]
                self.assertEqual(hashlib.sha256(output.read_bytes()).hexdigest(), digest, folder)
                self.assertEqual(manifest[key + "Sha256"], digest)
                rate, samples = read_wav(output)
                self.assertEqual(rate, 48000)
                self.assertEqual(len(samples), 8640)
                self.assertTrue(np.isfinite(samples).all())
            for key, value in provenance["currentStereoPairFiles"].items():
                self.assertEqual(manifest[key], value)


if __name__ == "__main__":
    unittest.main()
