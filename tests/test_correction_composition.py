"""Check cascade equivalence, input preservation, and the approved composed correction pairs."""
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

    def test_approved_composed_pairs_in_plugin_order(self):
        # Freeze the listening-approved release assets by head identity. This
        # catches swapping valid correction files between head packages.
        approved = {'IRCAM_1050': ['d82e500be6f3b5aa03e78a732b2502e6ab4de98cf94a8776ba9b9b77e8d96f7d', '55522b7727cb8c888050e806c4c1af7422cae2e2d308ed9ee69fd431e9e7f681'], 'MIT_KEMAR_Normal': ['0cf41d3a5dba9742805d77bf7de05e4b2e50c25bb1c41cc0efde7cb5bfd08705', 'cfdcd48d7060af0f11982a68b1b57cb85a167740378feebacf1fc463d48d86ed'], 'KU100_SADIE_D1': ['847f5eade8ef4ef07a5d40ddbf0e0ff6ec10b2057c63d7221d3fb5b60fdccb8f', '3de19222e0e7df4df35099a2771423b06efc1d5ec71fb8db36ef2da9620949c0'], 'KU100_FULL2DEG': ['78f6cd5b413da74afc648084aabc924ec311d6b7a1a223e515fbad5e2aec15d3', 'e964bcc077d9ddef4367efc7f418672adc1f56fb761e7cfc3e897f87cb085e72'], 'FABIAN_HATO0': ['934e0188f7e1dbd26c2545ccaf075a1be516c34c429616ea22d4e967e92d5960', 'bb599aae060663186eff79747cf7ba5a7b603b9356442766413fd90fcac35c7f']}
        for folder, digests in approved.items():
            head = ROOT / "Heads" / folder
            manifest = json.loads((head / "manifest.json").read_text(encoding="utf-8"))
            provenance = json.loads((head / "provenance.json").read_text(encoding="utf-8"))
            composition = provenance["stereoPairPostCorrection"]
            self.assertEqual(composition["outputTaps"], 8640)
            self.assertEqual(composition["tailFadeSamples"], 1920)
            self.assertEqual(composition["fullConvolutionTaps"], 17279)
            for side, digest in zip(("Left", "Right"), digests):
                key = "stereoPair" + side + "Correction"
                output = head / manifest[key + "File"]
                self.assertEqual(hashlib.sha256(output.read_bytes()).hexdigest(), digest, folder)
                self.assertEqual(manifest[key + "Sha256"], digest)
                rate, samples = read_wav(output)
                self.assertEqual(rate, 48000)
                self.assertEqual(len(samples), 8640)
                self.assertTrue(np.isfinite(samples).all())
                self.assertEqual(float(samples[-1]), 0.0)
            for key, value in provenance["currentStereoPairFiles"].items():
                self.assertEqual(manifest[key], value)


if __name__ == "__main__":
    unittest.main()
