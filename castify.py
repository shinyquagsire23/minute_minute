#!/usr/bin/env python3
# pip3 install pycryptodome

import sys, os, struct
import __future__

from base64 import b16decode
from Crypto.Cipher import AES
from Crypto.Hash import SHA

key = b16decode(b"00000000000000000000000000000000")
iv = b16decode(b"00000000000000000000000000000000")

no_crypto = True

loaderfile = sys.argv[1]
elffile = sys.argv[2]
outfile = sys.argv[3]
hybrid_mbr_ancast = sys.argv[4].lower() == "true"

print("Building payload...\n")

data = open(loaderfile, "rb").read()

hdrlen, loaderlen, elflen, arg = struct.unpack(">IIII", data[:16])

if hdrlen < 0x10:
    print("ERROR: header length is 0x%X, expected at least 0x10." % hdrlen)
    sys.exit(1)

loaderoff = hdrlen
elfoff = loaderoff + loaderlen
elfend = elfoff + elflen

hdr = data[:hdrlen]
loader = data[loaderoff:elfoff]

elf = open(elffile,"rb").read()

if elflen > 0:
    print("WARNING: loader already contains ELF, will replace.")

elflen = len(elf)

if loaderlen < len(loader):
    print("ERROR: loader is larger than its reported length.")
    sys.exit(1)

if loaderlen > len(loader):
    print("Padding loader with 0x%X zeroes." % (loaderlen - len(loader)))
    loader += b"\x00" * (loaderlen - len(loader))

print("Header size: 0x%X bytes." % hdrlen)
print("Loader size: 0x%X bytes." % loaderlen)
print("ELF size:    0x%X bytes." % elflen)

if hybrid_mbr_ancast:
    hdrlen = 0xEA000002

payload = struct.pack(">IIII", hdrlen, loaderlen, elflen, 0) + hdr[16:]
payload += loader
payload += elf

print("\nBuilding ancast image...\n")

hb_flags = 0x0000

payloadlen = len(payload)
if ((payloadlen + 0xFFF) & ~0xFFF) > payloadlen:
    print("Padding payload with 0x%X zeroes." % (((payloadlen + 0xFFF) & ~0xFFF) - payloadlen))
    payload += b"\x00" * (((payloadlen + 0xFFF) & ~0xFFF) - payloadlen)

if no_crypto:
    hb_flags |= 0b1
else:
    c = AES.new(key, AES.MODE_CBC, iv)
    payload = c.encrypt(payload)

h = SHA.new(payload)

if hybrid_mbr_ancast:
    outdata = struct.pack(">I4xI20xI256x124xHBBIII15sBBBBBBBBBBBBBB49xBB", 0xEFA282D9, 0x20, 0x02, hb_flags, 0x00, 0x00, 0x21, 0x02, len(payload), bytes([0]*15), 0x01, 0x41, 0x01, 0x0B, 0xFE, 0xC2, 0xFF, 0x00, 0x00, 0x02, 0x00, 0x00, 0x40, 0x1C, 0x55, 0xAA)
else:
    outdata = struct.pack(">I4xI20xI256x124xHBBIII20sI56x", 0xEFA282D9, 0x20, 0x02, hb_flags, 0x00, 0x00, 0x21, 0x02, len(payload), h.digest(), 0x02)

outdata += payload

print("Body size:   0x%X bytes." % len(payload))
print("Body hash:   %s." % h.hexdigest())

f = open(outfile, "wb")
f.write(outdata)
if hybrid_mbr_ancast:
    f.write(b'\x00' * ((0x80*0x200) - len(outdata)))
f.close()
