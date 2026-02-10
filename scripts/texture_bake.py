# Script to take an image file and convert it into a .bin file
#
# Uses ETC2 compression to minimize VRAM usage
# ETC2 is supported by OpenGL ES 3.0 and newer

import argparse
from PIL import Image
import etcpak, os
import struct

import numpy as np

def validate_file_extension(filepath, *exts):
    ext = os.path.splitext(filepath)[-1]
    if not ext.lower() in exts:
        raise argparse.ArgumentTypeError(
            f"File does not have the expected extensions: {exts}"
        )
    return filepath

def imgs_file(filepath):
    return validate_file_extension(filepath, ".png",".jpg",".jpeg")

def bin_file(filepath):
    return validate_file_extension(filepath, ".bin")

parser = argparse.ArgumentParser(
        prog="Texture Baker",
        description="Converts texture images (png/jpeg,etc.) into an optimized image format"
        )
parser.add_argument("-f", "--file", help="Path to the image file to convert", required=True, type=imgs_file)
parser.add_argument("-o", "--output", help="Path to write the converted image", required=True, type=bin_file)

# Premultiply alpha
parser.add_argument("-p", "--prealpha", help="Premultiply alpha color values", action="store_true")

args = parser.parse_args()
print(f"Converting {args.file} to {args.output}")

print("Reading...")
img = Image.open(args.file)
img = img.crop((0,0,img.width+img.width%4,img.height+img.height%4))
img = img.convert("RGBA") # Ensure RGBA

if args.prealpha:
    print("Premultiplying alpha...")
    img = img.convert("RGBa") # Pillow handles this with the RGBa mode
    img = img.convert("RGBA") # Back to RGBA

img_data = img.tobytes()

print("Converting...")
compressed = etcpak.compress_etc2_rgba(img_data, img.width, img.height)

# GL constant for COMPRESSED_SRGB8_ALPHA8_ETC2_EAC
gl_internal_format = 0x9279

data_size = len(compressed)

header = struct.pack("IIIII", 0x58455442, img.width, img.height, gl_internal_format, data_size)

print("Writing...")
with open(args.output, "wb") as f:
    f.write(header)
    f.write(compressed)

print(f"Success. Outputted to {args.output} ({img.width},{img.height})")
