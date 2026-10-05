"""Check cascade equivalence, input preservation, and the shipped residual recipes."""
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

    def test_shipped_components_hashes_gains_and_complex_response(self):
        for folder in ("IRCAM_1050", "KU100_SADIE_D1", "KU100_FULL2DEG"):
            head = ROOT / "Heads" / folder
            manifest = json.loads((head / "manifest.json").read_text(encoding="utf-8-sig"))
            provenance = json.loads((head / "provenance.json").read_text(encoding="utf-8-sig"))
            recipe = provenance["stereoPairResidualComposition"]
            for key, value in recipe["preservedGainFields"].items():
                self.assertEqual(manifest.get(key), value, (folder, key))
            components = ROOT / recipe["componentsDirectory"]
            for name, digest in recipe["componentSha256"].items():
                self.assertEqual(hashlib.sha256((components / name).read_bytes()).hexdigest(), digest)
            rate, extra = read_wav(components / "residual.wav")
            n = 65536
            for side in ("left", "right"):
                sr, base = read_wav(components / ("base_" + side + ".wav"))
                out = head / manifest["stereoPair" + side.title() + "CorrectionFile"]
                final_sr, final = read_wav(out)
                self.assertEqual((sr, final_sr), (rate, rate))
                self.assertEqual(len(final), len(base) + len(extra) - 1)
                self.assertTrue(np.isfinite(final).all())
                cascade = np.fft.rfft(base, n) * np.fft.rfft(extra, n)
                untapered = np.fft.irfft(cascade, n)[:len(final)]
                fade = recipe["results"][side]["tailFadeSamples"]
                self.assertEqual(fade, round(rate * .040))
                expected_samples = untapered.copy()
                expected_samples[-fade:] *= .5 * (1 + np.cos(np.linspace(0, np.pi, fade)))
                np.testing.assert_allclose(final, expected_samples, rtol=1e-4, atol=2e-7)
                self.assertEqual(final[-1], 0.0)
                expected = np.fft.rfft(expected_samples, n)
                actual = np.fft.rfft(final, n)
                np.testing.assert_allclose(actual, expected, rtol=1e-4, atol=2e-6)
                frequencies = np.fft.rfftfreq(n, 1 / rate)
                band = (frequencies >= 20) & (frequencies <= 20000)
                delta = 20 * np.log10(np.abs(actual[band]) / np.abs(cascade[band]))
                self.assertLess(np.max(np.abs(delta)), .001)
                key = "stereoPair" + side.title() + "CorrectionSha256"
                self.assertEqual(hashlib.sha256(out.read_bytes()).hexdigest(), manifest[key])


if __name__ == "__main__":
    unittest.main()
