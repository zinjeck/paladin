"""Verify every ICO image embedded in a Windows executable, without executing it.

Usage: python tools/check_windows_icon.py Paladin.exe assets/icons/paladin.ico
Uses only the Python standard library and Win32's data-only resource loader.
"""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import struct


def ico_images(data: bytes) -> list[tuple[int, int, bytes]]:
    if len(data) < 6:
        raise ValueError("Truncated ICO header")
    reserved, kind, count = struct.unpack_from("<HHH", data)
    if reserved or kind != 1 or not count or 6 + 16 * count > len(data):
        raise ValueError("Invalid ICO directory")
    images = []
    for i in range(count):
        width, height, _, _, _, _, size, offset = struct.unpack_from(
            "<BBBBHHII", data, 6 + 16 * i)
        if not size or offset < 6 + 16 * count or offset + size > len(data):
            raise ValueError(f"Invalid ICO image {i}")
        images.append((width or 256, height or 256, data[offset:offset + size]))
    return images


def verify(executable: Path, icon: Path) -> dict:
    expected = ico_images(icon.read_bytes())
    if os.name != "nt":
        raise RuntimeError("Executable resource verification requires Windows")
    from ctypes import wintypes
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.LoadLibraryExW.argtypes = (wintypes.LPCWSTR, ctypes.c_void_p, wintypes.DWORD)
    kernel.LoadLibraryExW.restype = ctypes.c_void_p
    kernel.FreeLibrary.argtypes = (ctypes.c_void_p,)
    kernel.FreeLibrary.restype = wintypes.BOOL
    kernel.FindResourceW.argtypes = (ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
    kernel.FindResourceW.restype = ctypes.c_void_p
    kernel.SizeofResource.argtypes = (ctypes.c_void_p, ctypes.c_void_p)
    kernel.SizeofResource.restype = wintypes.DWORD
    kernel.LoadResource.argtypes = (ctypes.c_void_p, ctypes.c_void_p)
    kernel.LoadResource.restype = ctypes.c_void_p
    kernel.LockResource.argtypes = (ctypes.c_void_p,)
    kernel.LockResource.restype = ctypes.c_void_p
    # LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE: no entry point,
    # no DLL initialization, and no need to locate the application's SDL DLLs.
    module = kernel.LoadLibraryExW(str(executable.resolve()), None, 0x02 | 0x20)
    if not module:
        raise ctypes.WinError(ctypes.get_last_error())
    try:
        def resource(kind: int, identifier: int) -> bytes:
            found = kernel.FindResourceW(module, identifier, kind)
            if not found:
                raise ctypes.WinError(ctypes.get_last_error())
            size = kernel.SizeofResource(module, found)
            loaded = kernel.LoadResource(module, found)
            address = kernel.LockResource(loaded) if loaded else None
            if not address or not size:
                raise ValueError(f"Empty icon resource {kind}/{identifier}")
            return ctypes.string_at(address, size)

        group = resource(14, 1)  # RT_GROUP_ICON, the application's resource ID.
        if len(group) < 6:
            raise ValueError("Truncated embedded icon group")
        reserved, kind, count = struct.unpack_from("<HHH", group)
        if reserved or kind != 1 or count != len(expected) or len(group) != 6 + 14 * count:
            raise ValueError("Embedded icon group does not match the ICO directory")
        remaining = expected.copy()
        evidence = []
        for i in range(count):
            width, height, _, _, _, _, size, identifier = struct.unpack_from(
                "<BBBBHHIH", group, 6 + 14 * i)
            payload = resource(3, identifier)  # RT_ICON
            image = (width or 256, height or 256, payload)
            if len(payload) != size or image not in remaining:
                raise ValueError(f"Embedded icon image {identifier} differs from supplied artwork")
            remaining.remove(image)
            evidence.append({"width": image[0], "height": image[1], "resource": identifier,
                             "bytes": size, "sha256": hashlib.sha256(payload).hexdigest()})
        if remaining:
            raise ValueError("Supplied ICO images missing from executable")
        return {"executable": str(executable), "icon": str(icon), "matched_images": count,
                "source_sha256": hashlib.sha256(icon.read_bytes()).hexdigest(), "images": evidence}
    finally:
        kernel.FreeLibrary(module)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("icon", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    try:
        report = verify(args.executable, args.icon)
        text = json.dumps(report, indent=2)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            args.report.write_text(text + "\n", encoding="utf-8")
        print(text)
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f"Icon verification failed: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
