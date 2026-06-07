import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))

from hsd.run import find_binary, build_batch_command


def test_find_binary_prefers_env(monkeypatch, tmp_path):
    fake = tmp_path / "hesiod"
    fake.write_text("")
    fake.chmod(0o755)
    monkeypatch.setenv("HESIOD_BIN", str(fake))
    assert find_binary() == str(fake)


def test_build_batch_command_includes_overrides():
    cmd = build_batch_command("/bin/hesiod", "g.hsd", shape=[512, 512],
                              tiling=[4, 4], overlap=0.25)
    assert cmd[0] == "/bin/hesiod"
    assert "--batch=g.hsd" in cmd
    assert "--shape=512,512" in cmd
    assert "--tiling=4,4" in cmd
    assert "--overlap=0.25" in cmd


def test_build_batch_command_minimal():
    cmd = build_batch_command("/bin/hesiod", "g.hsd")
    assert cmd == ["/bin/hesiod", "--batch=g.hsd"]
