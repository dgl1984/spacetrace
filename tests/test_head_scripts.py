#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"

try:
    import h5py
    import numpy as np
except Exception:
    h5py = None
    np = None


def run(*args, python=None, cwd=ROOT):
    exe = python or sys.executable
    return subprocess.run([exe, *map(str,args)], cwd=cwd, text=True, capture_output=True)


def write_sofa(path: Path, *, convention="SimpleFreeFieldHRIR", receivers=2, sr=48000.0):
    if h5py is None:
        raise unittest.SkipTest("h5py/numpy unavailable")
    m, taps = 4, 16
    with h5py.File(path, "w") as f:
        f.attrs["SOFAConventions"] = convention
        f.attrs["Title"] = "Synthetic Head"
        data = f.create_group("Data")
        ir = np.zeros((m, receivers, taps), dtype=np.float64)
        if receivers >= 1:
            ir[:,0,0] = 1.0
        if receivers >= 2:
            ir[:,1,1] = 0.8
        data.create_dataset("IR", data=ir)
        data.create_dataset("SamplingRate", data=np.asarray([sr], dtype=np.float64))
        pos = f.create_dataset("SourcePosition", data=np.asarray([
            [0.0, 0.0, 1.0], [90.0, 0.0, 1.0], [180.0, 0.0, 1.0], [-90.0, 0.0, 1.0]
        ], dtype=np.float64))
        pos.attrs["Type"] = "spherical"
        pos.attrs["Units"] = "degree, degree, meter"


def write_float_wav(path: Path, sr=48000, values=None):
    if values is None:
        values = [1.0] + [0.0] * 31
    raw = b"".join(struct.pack("<f", float(x)) for x in values)
    fmt = struct.pack("<HHIIHH", 3, 1, sr, sr*4, 4, 32)
    fact = struct.pack("<I", len(values))
    riff_size = 4 + (8+len(fmt)) + (8+len(fact)) + (8+len(raw))
    path.write_bytes(b"RIFF"+struct.pack("<I",riff_size)+b"WAVE"+b"fmt "+struct.pack("<I",len(fmt))+fmt+b"fact"+struct.pack("<I",len(fact))+fact+b"data"+struct.pack("<I",len(raw))+raw)


@unittest.skipIf(h5py is None, "h5py/numpy unavailable")
class ScriptTests(unittest.TestCase):
    def setUp(self):
        self.tmp_obj = tempfile.TemporaryDirectory(prefix="spacetrace-script-test-")
        self.tmp = Path(self.tmp_obj.name)
        self.sofa = self.tmp / "good.sofa"
        write_sofa(self.sofa)
        self.heads = self.tmp / "Heads"
        self.head = self.heads / "Synthetic"

    def tearDown(self):
        self.tmp_obj.cleanup()

    def convert(self, *, stable="test.synthetic.48000", out=None, extra=()):
        out = out or self.head
        return run(SCRIPTS/"convert_sofa_head.py", self.sofa, out, "--stable-id", stable, "--display-name", "Synthetic", *extra)

    def test_successful_raw_conversion_and_validation(self):
        p = self.convert()
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        self.assertIn("Head package creation: PASS", p.stdout)
        v = run(SCRIPTS/"validate_head.py", self.head, "--heads-root", self.heads)
        self.assertEqual(v.returncode, 0, v.stderr + v.stdout)
        self.assertIn("correction: none (Raw-only; this is valid)", v.stdout)

    def test_missing_input_has_actionable_error(self):
        p = run(SCRIPTS/"convert_sofa_head.py", self.tmp/"wrong-name.sofa", self.head, "--stable-id", "test.synthetic.48000")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("ERROR:", p.stderr)
        self.assertIn("HOW TO FIX:", p.stderr)
        self.assertIn("was not found", p.stderr)

    def test_missing_dependencies_are_explained(self):
        # -S disables site-packages while retaining the standard library.
        p = run("-S", SCRIPTS/"convert_sofa_head.py", self.sofa, self.head, "--stable-id", "test.synthetic.48000")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Missing Python package", p.stderr)
        self.assertIn("requirements.txt", p.stderr)

    def test_non_sofa_file_is_explained(self):
        bogus = self.tmp / "not-sofa.sofa"; bogus.write_text("hello", encoding="utf-8")
        p = run(SCRIPTS/"convert_sofa_head.py", bogus, self.head, "--stable-id", "test.synthetic.48000")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Could not open SOFA", p.stderr)
        self.assertIn("HOW TO FIX:", p.stderr)

    def test_unsupported_convention_is_rejected(self):
        bad = self.tmp / "bad-convention.sofa"; write_sofa(bad, convention="FreeFieldDirectivityTF")
        p = run(SCRIPTS/"convert_sofa_head.py", bad, self.head, "--stable-id", "test.synthetic.48000")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Unsupported SOFA convention", p.stderr)

    def test_non_binaural_source_is_rejected(self):
        bad = self.tmp / "three-ear.sofa"; write_sofa(bad, receivers=3)
        p = run(SCRIPTS/"convert_sofa_head.py", bad, self.head, "--stable-id", "test.synthetic.48000")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("exactly two receivers", p.stderr)

    def test_bad_stable_id_is_rejected(self):
        p = self.convert(stable="bad id with spaces")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Invalid stable ID", p.stderr)

    def test_duplicate_stable_id_is_rejected(self):
        first = self.convert()
        self.assertEqual(first.returncode, 0, first.stderr)
        other = self.heads / "Other"
        p = self.convert(out=other)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("already used", p.stderr)

    def test_existing_output_is_refused_without_overwrite(self):
        self.assertEqual(self.convert().returncode, 0)
        p = self.convert()
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("already exists", p.stderr)
        self.assertIn("--overwrite", p.stderr)

    def test_output_parent_that_is_a_file_is_explained(self):
        parent = self.tmp / "not-a-dir"; parent.write_text("x", encoding="utf-8")
        p = self.convert(out=parent/"Head")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("not a directory", p.stderr)

    def test_malformed_manifest_is_explained(self):
        self.assertEqual(self.convert().returncode, 0)
        (self.head/"manifest.json").write_text("{ this is not json", encoding="utf-8")
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertNotEqual(v.returncode, 0)
        self.assertIn("Could not read JSON", v.stderr)
        self.assertIn("HOW TO FIX:", v.stderr)

    def test_manifest_hash_mismatch_is_detected(self):
        self.assertEqual(self.convert().returncode, 0)
        manifest = json.loads((self.head/"manifest.json").read_text())
        manifest["headSha256"] = "0"*64
        (self.head/"manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertNotEqual(v.returncode, 0)
        self.assertIn("Head SHA-256 mismatch", v.stderr)

    def test_missing_referenced_correction_is_detected(self):
        self.assertEqual(self.convert().returncode, 0)
        manifest = json.loads((self.head/"manifest.json").read_text())
        manifest.update(correctionFile="missing.wav", correctionSha256="0"*64)
        (self.head/"manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertNotEqual(v.returncode, 0)
        self.assertIn("Correction file referenced by manifest was not found", v.stderr)

    def test_invalid_correction_is_refused_before_packaging(self):
        self.assertEqual(self.convert().returncode, 0)
        bad = self.tmp/"bad.wav"; bad.write_text("not wave", encoding="utf-8")
        p = run(SCRIPTS/"package_head.py", self.head, "--correction", bad)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("not a RIFF/WAVE", p.stderr)
        self.assertFalse((self.head/"correction.wav").exists())

    def test_correction_sample_rate_mismatch_is_supported(self):
        self.assertEqual(self.convert().returncode, 0)
        wav = self.tmp/"other-rate.wav"; write_float_wav(wav, sr=44100)
        p = run(SCRIPTS/"package_head.py", self.head, "--correction", wav)
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        self.assertIn("runtime resampling", p.stdout)
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertEqual(v.returncode, 0, v.stderr + v.stdout)

    def test_normalizer_refuses_existing_output(self):
        wav=self.tmp/"in.wav"; out=self.tmp/"out.wav"; write_float_wav(wav); write_float_wav(out)
        p=run(SCRIPTS/"normalize_correction.py", wav, out)
        self.assertNotEqual(p.returncode,0)
        self.assertIn("already exists",p.stderr)

    def test_normalize_then_package_then_validate(self):
        self.assertEqual(self.convert().returncode, 0)
        wav=self.tmp/"correction.wav"; norm=self.tmp/"neutral.wav"
        # A simple low-pass-ish FIR with non-unity overall gain.
        write_float_wav(wav, values=[0.5,0.25,0.125,0.0625]+[0.0]*28)
        n=run(SCRIPTS/"normalize_correction.py",wav,norm)
        self.assertEqual(n.returncode,0,n.stderr+n.stdout)
        self.assertIn("pink-noise RMS neutral",n.stdout)
        p=run(SCRIPTS/"package_head.py",self.head,"--correction",norm,"--correction-name","Synthetic correction","--level-trim-db","-1.25")
        self.assertEqual(p.returncode,0,p.stderr+p.stdout)
        v=run(SCRIPTS/"validate_head.py",self.head)
        self.assertEqual(v.returncode,0,v.stderr+v.stdout)
        self.assertIn("fixed level trim: -1.25 dB",v.stdout)
        self.assertIn("correction: PASS",v.stdout)

    def test_channel_analysis_reports_and_writes_equal_opposite_gains(self):
        self.assertEqual(self.convert().returncode, 0)
        p = run(SCRIPTS/"analyze_head_channels.py", self.head, "--write-manifest")
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        self.assertIn("front-center channel analysis: PASS", p.stdout)
        self.assertIn("measured L-R imbalance", p.stdout)
        manifest = json.loads((self.head/"manifest.json").read_text())
        self.assertIn("leftGainDb", manifest); self.assertIn("rightGainDb", manifest)
        self.assertAlmostEqual(float(manifest["leftGainDb"]) + float(manifest["rightGainDb"]), 0.0, places=5)
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertEqual(v.returncode, 0, v.stderr + v.stdout)
        self.assertIn("fixed channel gains", v.stdout)

    def test_validator_rejects_non_neutral_channel_gain_pair(self):
        self.assertEqual(self.convert().returncode, 0)
        manifest = json.loads((self.head/"manifest.json").read_text())
        manifest["leftGainDb"] = 1.0; manifest["rightGainDb"] = 0.0
        (self.head/"manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertNotEqual(v.returncode, 0)
        self.assertIn("equal-and-opposite", v.stderr)

    def test_plot_correction_wav_creates_png_and_description(self):
        wav = self.tmp/"plot.wav"; png = self.tmp/"plot.png"
        write_float_wav(wav, values=[1.0, -0.25, 0.125] + [0.0]*61)
        p = run(SCRIPTS/"plot_correction.py", wav, png, "--title", "Synthetic correction")
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        self.assertTrue(png.is_file() and png.stat().st_size > 1000)
        desc = png.with_suffix(".txt")
        self.assertTrue(desc.is_file())
        text = desc.read_text()
        self.assertIn("Strongest boost", text); self.assertIn("Deepest cut", text)

    def test_plot_correction_ttm_creates_png_and_description(self):
        ttm = self.tmp/"model.ttm"; png = self.tmp/"model.png"
        lines = ["ToneTraceModel 1", "mode voice", "range 20 20000", "resolution 30", "nodes 8"]
        for f,g in [(20,-1),(50,-0.5),(100,0),(500,1),(1000,-2),(4000,3),(10000,-1),(20000,2)]:
            lines.append(f"{f} {g} 0.9")
        ttm.write_text("\n".join(lines)+"\n")
        p = run(SCRIPTS/"plot_correction.py", ttm, png)
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        self.assertTrue(png.is_file())
        desc = png.with_suffix(".txt").read_text()
        self.assertIn("Tone Trace model nodes", desc)
        self.assertIn("+3.00 dB at approximately 4000 Hz", desc)
        self.assertIn("-2.00 dB at approximately 1000 Hz", desc)

    def test_plot_correction_rejects_bad_extension(self):
        src = self.tmp/"curve.csv"; src.write_text("1,2")
        p = run(SCRIPTS/"plot_correction.py", src, self.tmp/"x.png")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Unsupported correction source extension", p.stderr)
        self.assertIn("HOW TO FIX:", p.stderr)

    def test_plot_correction_rejects_malformed_ttm(self):
        src = self.tmp/"bad.ttm"; src.write_text("not a tone trace model\n")
        p = run(SCRIPTS/"plot_correction.py", src, self.tmp/"x.png")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Not a recognized Tone Trace model", p.stderr)

    def test_plot_correction_refuses_overwrite(self):
        wav = self.tmp/"plot.wav"; png = self.tmp/"plot.png"
        write_float_wav(wav); png.write_bytes(b"existing")
        p = run(SCRIPTS/"plot_correction.py", wav, png)
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Output already exists", p.stderr)


    def test_plot_correction_missing_dependencies_are_explained(self):
        wav = self.tmp/"plot.wav"; write_float_wav(wav)
        p = run("-S", SCRIPTS/"plot_correction.py", wav, self.tmp/"plot.png")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Missing Python package", p.stderr)
        self.assertIn("requirements.txt", p.stderr)

    def test_channel_analyzer_missing_head_folder_is_explained(self):
        p = run(SCRIPTS/"analyze_head_channels.py", self.tmp/"NoSuchHead")
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("Head folder not found", p.stderr)
        self.assertIn("HOW TO FIX:", p.stderr)

    def test_packager_can_calibrate_front_center(self):
        self.assertEqual(self.convert().returncode, 0)
        p = run(SCRIPTS/"package_head.py", self.head, "--calibrate-front-center")
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        manifest = json.loads((self.head/"manifest.json").read_text())
        self.assertIn("frontCenterImbalanceDb", manifest)
        self.assertAlmostEqual(float(manifest["leftGainDb"]) + float(manifest["rightGainDb"]), 0.0, places=5)


    def test_prepare_stereo_pair_normalizes_packages_and_validates(self):
        self.assertEqual(self.convert().returncode, 0)
        left = self.tmp/"left90.wav"; right = self.tmp/"right270.wav"
        write_float_wav(left, values=[0.5, 0.25, 0.125] + [0.0]*29)
        write_float_wav(right, values=[0.4, 0.2, 0.1] + [0.0]*29)
        p = run(SCRIPTS/"prepare_stereo_pair.py", self.head, left, right)
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        self.assertIn("Stereo Pair preparation: PASS", p.stdout)
        manifest = json.loads((self.head/"manifest.json").read_text())
        self.assertEqual(manifest["stereoPairLeftCorrectionFile"], "stereo_pair_left_correction.wav")
        self.assertEqual(manifest["stereoPairRightCorrectionFile"], "stereo_pair_right_correction.wav")
        self.assertAlmostEqual(float(manifest["stereoPairLeftGainDb"]) + float(manifest["stereoPairRightGainDb"]), 0.0, places=5)
        self.assertIn("stereoPairTrimDb", manifest)
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertEqual(v.returncode, 0, v.stderr + v.stdout)
        self.assertIn("Stereo Pair corrections: PASS", v.stdout)

    def test_validator_rejects_incomplete_stereo_pair_metadata(self):
        self.assertEqual(self.convert().returncode, 0)
        left = self.tmp/"left.wav"; write_float_wav(left)
        shutil.copy2(left, self.head/"stereo_pair_left_correction.wav")
        manifest = json.loads((self.head/"manifest.json").read_text())
        manifest["stereoPairLeftCorrectionFile"] = "stereo_pair_left_correction.wav"
        manifest["stereoPairLeftCorrectionSha256"] = "0"*64
        (self.head/"manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertNotEqual(v.returncode, 0)
        self.assertIn("both left and right", v.stderr)

    def test_validator_rejects_non_neutral_stereo_pair_source_gains(self):
        self.assertEqual(self.convert().returncode, 0)
        left = self.tmp/"left90.wav"; right = self.tmp/"right270.wav"
        write_float_wav(left); write_float_wav(right)
        p = run(SCRIPTS/"prepare_stereo_pair.py", self.head, left, right)
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        manifest = json.loads((self.head/"manifest.json").read_text())
        manifest["stereoPairLeftGainDb"] = 1.0
        manifest["stereoPairRightGainDb"] = 0.0
        (self.head/"manifest.json").write_text(json.dumps(manifest), encoding="utf-8")
        v = run(SCRIPTS/"validate_head.py", self.head)
        self.assertNotEqual(v.returncode, 0)
        self.assertIn("equal-and-opposite", v.stderr)

    def test_remove_correction_also_removes_stereo_pair_assets(self):
        self.assertEqual(self.convert().returncode, 0)
        mono = self.tmp/"mono.wav"; left = self.tmp/"left90.wav"; right = self.tmp/"right270.wav"
        write_float_wav(mono); write_float_wav(left); write_float_wav(right)
        self.assertEqual(run(SCRIPTS/"package_head.py", self.head, "--correction", mono).returncode, 0)
        self.assertEqual(run(SCRIPTS/"prepare_stereo_pair.py", self.head, left, right).returncode, 0)
        p = run(SCRIPTS/"package_head.py", self.head, "--remove-correction")
        self.assertEqual(p.returncode, 0, p.stderr + p.stdout)
        manifest = json.loads((self.head/"manifest.json").read_text())
        self.assertNotIn("correctionFile", manifest)
        self.assertNotIn("stereoPairLeftCorrectionFile", manifest)
        self.assertFalse((self.head/"stereo_pair_left_correction.wav").exists())
        self.assertFalse((self.head/"stereo_pair_right_correction.wav").exists())



if __name__ == "__main__":
    unittest.main(verbosity=2)
