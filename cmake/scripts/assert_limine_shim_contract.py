#!/usr/bin/env python3
"""Validate the Limine shim link address against the kernel address contract."""

import argparse
import struct
import sys

PT_LOAD = 1


def parse_int(value: str) -> int:
    return int(value, 0)


def fail(message: str) -> None:
    print(message, file=sys.stderr)
    sys.exit(1)


def read_program_headers(path: str):
    with open(path, "rb") as f:
        data = f.read()

    if len(data) < 64 or data[:4] != b"\x7fELF":
        fail(f"Limine shim contract check: {path} is not an ELF file")
    if data[4] != 2 or data[5] != 1:
        fail("Limine shim contract check: expected little-endian ELF64")

    header = struct.unpack_from("<16sHHIQQQIHHHHHH", data, 0)
    entry = header[4]
    phoff = header[5]
    phentsize = header[9]
    phnum = header[10]
    if phentsize != 56:
        fail("Limine shim contract check: unexpected ELF64 program-header size")

    headers = []
    for index in range(phnum):
        offset = phoff + index * phentsize
        if offset + phentsize > len(data):
            fail("Limine shim contract check: program header table runs past end of file")
        ph = struct.unpack_from("<IIQQQQQQ", data, offset)
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = ph
        headers.append(
            {
                "type": p_type,
                "flags": p_flags,
                "offset": p_offset,
                "vaddr": p_vaddr,
                "paddr": p_paddr,
                "filesz": p_filesz,
                "memsz": p_memsz,
                "align": p_align,
            }
        )
    return entry, headers, len(data)


def ranges_overlap(left_start: int, left_end: int, right_start: int, right_end: int) -> bool:
    return left_start < right_end and right_start < left_end


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True)
    parser.add_argument("--expected-base", required=True, type=parse_int)
    parser.add_argument("--kernel-virtual-offset", required=True, type=parse_int)
    parser.add_argument("--kernel-reserved-start", required=True, type=parse_int)
    parser.add_argument("--kernel-reserved-end", required=True, type=parse_int)
    args = parser.parse_args()

    entry, headers, file_size = read_program_headers(args.elf)
    load_ranges = []
    for header in headers:
        if header["type"] != PT_LOAD:
            continue
        if header["memsz"] < header["filesz"]:
            fail("Limine shim contract check: PT_LOAD memsz is smaller than filesz")
        if (header["offset"] + header["filesz"]) > file_size:
            fail("Limine shim contract check: PT_LOAD file range runs past end of file")
        load_ranges.append((header["vaddr"], header["vaddr"] + header["memsz"]))

    if not load_ranges:
        fail("Limine shim contract check: shim ELF has no PT_LOAD segments")

    shim_start = min(start for start, _ in load_ranges)
    shim_end = max(end for _, end in load_ranges)
    if shim_start != args.expected_base:
        fail(
            "Limine shim contract check: "
            f"shim starts at 0x{shim_start:x}, expected 0x{args.expected_base:x}"
        )
    if not (shim_start <= entry < shim_end):
        fail(
            "Limine shim contract check: "
            f"entry point 0x{entry:x} is outside shim load range 0x{shim_start:x}-0x{shim_end:x}"
        )

    kernel_start = args.kernel_virtual_offset + args.kernel_reserved_start
    kernel_end = args.kernel_virtual_offset + args.kernel_reserved_end
    if ranges_overlap(shim_start, shim_end, kernel_start, kernel_end):
        fail(
            "Limine shim contract check: "
            f"shim range 0x{shim_start:x}-0x{shim_end:x} overlaps shared kernel "
            f"window 0x{kernel_start:x}-0x{kernel_end:x}"
        )


if __name__ == "__main__":
    main()
