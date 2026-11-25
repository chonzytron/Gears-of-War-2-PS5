#!/usr/bin/env python3
# Gears of War 2 for PS5 - take one file out of an Xbox 360 disc image (XDVDFS / XGD) or an
# STFS package (a title update, LIVE / PIRS / CON), on the build machine. Used to stage the
# player's own default.xex and default.xexp for the code generator; the PS5 app does the same
# on the console with the SDK's DiscImageDevice / StfsContainerDevice.
#
#   x360_extract.py list <image-or-package>
#   x360_extract.py get  <image-or-package> <name-in-it> <output-file>
#   x360_extract.py info <image-or-package>          # title id, versions (STFS header)
#
# GPL-3.0 (the PS5 layer). Never commit what this extracts.
import hashlib
import struct
import sys

SECTOR = 2048
# Where the game partition starts in each disc format (XGD1, XGD2, XGD3, plain XISO).
PARTITION_OFFSETS = (0x18300000, 0xFD90000, 0x2080000, 0)
XDVDFS_MAGIC = b"MICROSOFT*XBOX*MEDIA"


class Xdvdfs:
    def __init__(self, f):
        self.f = f
        for base in PARTITION_OFFSETS:
            f.seek(base + 32 * SECTOR)
            vd = f.read(0x1C)
            if vd[:20] == XDVDFS_MAGIC:
                self.base = base
                self.root_sector, self.root_size = struct.unpack("<II", vd[20:28])
                return
        raise ValueError("not an Xbox 360 disc image")

    def _read(self, sector, size):
        self.f.seek(self.base + sector * SECTOR)
        return self.f.read(size)

    def _walk(self, sector, size, prefix):
        table = self._read(sector, size)
        seen = set()
        stack = [0]
        while stack:
            off = stack.pop()
            if off in seen or off + 14 > len(table):
                continue
            seen.add(off)
            left, right, start, length, attr, nlen = struct.unpack("<HHIIBB", table[off:off + 14])
            if left == 0xFFFF and right == 0xFFFF:
                continue
            name = table[off + 14:off + 14 + nlen].decode("latin-1")
            path = prefix + name
            if attr & 0x10:
                if length:
                    yield from self._walk(start, length, path + "/")
            else:
                yield path, start, length
            if left and left != 0xFFFF:
                stack.append(left * 4)
            if right and right != 0xFFFF:
                stack.append(right * 4)

    def files(self):
        return list(self._walk(self.root_sector, self.root_size, ""))

    def read_file(self, entry):
        _, start, length = entry
        return self._read(start, length)


class Stfs:
    def __init__(self, f):
        self.f = f
        h = f.read(0x971A)
        if h[:4] not in (b"LIVE", b"PIRS", b"CON "):
            raise ValueError("not an STFS package")
        self.header = h
        header_size = struct.unpack(">I", h[0x340:0x344])[0]
        vd = h[0x379:0x379 + 0x24]
        block_sep = vd[2]
        self.ft_blocks = struct.unpack("<H", vd[3:5])[0]
        self.ft_start = vd[5] | vd[6] << 8 | vd[7] << 16
        self.first = (header_size + 0xFFF) & 0xFFFFF000
        if ((header_size + 0xFFF) & 0xF000) >> 12 == 0xB or (block_sep & 1):
            self.shift = 0
        else:
            self.shift = 1
        # Blocks between hash tables, by level (one table per level, or two).
        self.step0 = 0xAB if self.shift == 0 else 0xAC
        self.step1 = 0x718F if self.shift == 0 else 0x723A

    def info(self):
        h = self.header
        name = h[0x411:0x411 + 0x80].decode("utf-16-be", "ignore").split("\0")[0]
        return {
            "magic": h[:4].decode(),
            "title_id": "%08X" % struct.unpack(">I", h[0x360:0x364])[0],
            "content_type": "%08X" % struct.unpack(">I", h[0x344:0x348])[0],
            "base_version": struct.unpack(">I", h[0x358:0x35C])[0],
            "version": struct.unpack(">I", h[0x35C:0x360])[0],
            "name": name,
        }

    def _backing(self, b):
        n = (((b + 0xAA) // 0xAA) << self.shift) + b
        if b < 0xAA:
            return n
        if b < 0x70E4:
            return n + (((b + 0x70E4) // 0x70E4) << self.shift)
        return (1 << self.shift) + n + (((b + 0x70E4) // 0x70E4) << self.shift)

    def _hash_block(self, b):
        n = (b // 0xAA) * self.step0
        if b >= 0xAA:
            n += ((b // 0x70E4) + 1) << self.shift
        if b >= 0x70E4:
            n += 1 << self.shift
        return n

    def _block(self, b):
        self.f.seek(self.first + self._backing(b) * 0x1000)
        return self.f.read(0x1000)

    def _next(self, b):
        self.f.seek(self.first + self._hash_block(b) * 0x1000 + (b % 0xAA) * 0x18)
        e = self.f.read(0x18)
        return e[0x15] << 16 | e[0x16] << 8 | e[0x17]

    def files(self):
        out, names = [], {}
        b = self.ft_start
        for _ in range(self.ft_blocks):
            blk = self._block(b)
            for i in range(0, 0x1000, 0x40):
                e = blk[i:i + 0x40]
                flags = e[0x28]
                nlen = flags & 0x3F
                if nlen == 0:
                    continue
                name = e[:nlen].decode("latin-1")
                start = e[0x2F] | e[0x30] << 8 | e[0x31] << 16
                parent = struct.unpack(">h", e[0x32:0x34])[0]
                size = struct.unpack(">I", e[0x34:0x38])[0]
                idx = len(names)
                full = (names.get(parent, "") + "/" if parent >= 0 else "") + name
                names[idx] = full
                if not flags & 0x80:
                    out.append((full, start, size, bool(flags & 0x40)))
            b = self._next(b)
        return out

    def read_file(self, entry):
        _, start, size, consecutive = entry
        data = bytearray()
        b = start
        while len(data) < size:
            data += self._block(b)
            b = b + 1 if consecutive else self._next(b)
        return bytes(data[:size])


def open_container(path):
    f = open(path, "rb")
    magic = f.read(4)
    f.seek(0)
    return Stfs(f) if magic in (b"LIVE", b"PIRS", b"CON ") else Xdvdfs(f)


def main(argv):
    if len(argv) < 3:
        print(__doc__ or "see the top of this file")
        return 2
    cmd, path = argv[1], argv[2]
    c = open_container(path)
    if cmd == "info":
        if isinstance(c, Stfs):
            for k, v in c.info().items():
                print(f"{k}: {v}")
        else:
            print(f"disc image, partition at 0x{c.base:X}")
        return 0
    if cmd == "list":
        for e in c.files():
            print(f"{e[2]:>12}  {e[0]}")
        return 0
    if cmd == "get" and len(argv) == 5:
        want = argv[3].replace("\\", "/").lower()
        for e in c.files():
            if e[0].lower() == want:
                data = c.read_file(e)
                with open(argv[4], "wb") as o:
                    o.write(data)
                print(f"{e[0]}: {len(data)} bytes, sha1 {hashlib.sha1(data).hexdigest()}, "
                      f"sha256 {hashlib.sha256(data).hexdigest()}")
                return 0
        print(f"{argv[3]} not found", file=sys.stderr)
        return 1
    print("unknown command", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
