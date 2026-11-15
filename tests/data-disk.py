#!/usr/bin/env python3
import importlib.util
from pathlib import Path
import tempfile


def load(name):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).parent.parent / "scripts" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


creator, disks = load("create-data-disk"), load("data-disk")
with tempfile.TemporaryDirectory() as directory:
    source, backup, restored = (Path(directory) / name for name in ("source.img", "backup.img", "restored.img"))
    creator.create(source)
    assert disks.inspect(source) == []
    disks.copy_validated(source, backup)
    disks.copy_validated(backup, restored)
    assert source.read_bytes() == restored.read_bytes()
    for operation in (lambda: creator.create(source), lambda: disks.copy_validated(backup, source)):
        try:
            operation()
            raise AssertionError("Existing image was overwritten")
        except FileExistsError:
            pass
    with restored.open("r+b") as image:
        image.seek(2048 * 512)
        image.write(b"torn generation")
    try:
        disks.inspect(restored)
        raise AssertionError("Corrupt image accepted")
    except ValueError:
        pass
print("PASS: image creation, backup/restore, corruption rejection and overwrite refusal")
