import struct, json, subprocess, os, argparse 
from PIL import Image

BOLD = "\033[1m"
BOLD_END = "\033[0m"

def build_font(ttf_path, output_bin, size=32, px_range=4):
    filename = os.path.basename(ttf_path).split(".")[0]
    out_dir = os.path.dirname(output_bin)
    
    # FIX 1: Use os.path.join for safe pathing
    temp_json = os.path.join(out_dir, filename + ".json")
    temp_png = os.path.join(out_dir, filename + ".png")

    print(BOLD + f"Generating MSDF Atlas for {ttf_path}..." + BOLD_END)
    cmd = [
        "msdf-atlas-gen",
        "-font", ttf_path,
        "-size", str(size),
        "-format", "png",
        "-type", "msdf",
        "-json", temp_json,
        "-imageout", temp_png,
        "-pxrange", str(px_range),
    ]
    
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error: msdf-atlas-gen failed")
        return

    print(BOLD + f"Packing binary into {output_bin}..." + BOLD_END)
    try:
        with open(temp_json, "r") as f:
            data = json.load(f)
        
        # FIX 2: Flip the image vertically. 
        # This makes the PIL buffer match OpenGL's bottom-up coordinate system.
        img = Image.open(temp_png).convert("RGB").transpose(Image.Transpose.FLIP_TOP_BOTTOM)
        pixels = img.tobytes()

        glyphs = { g["unicode"]: g for g in data["glyphs"] }

        with open(output_bin, "wb") as f:
            # Header (20 bytes)
            header = struct.pack("4s f I I I", b'FONT', float(px_range), img.width, img.height, 96)
            f.write(header)
            
            for i in range(32, 128):
                g = glyphs.get(i, glyphs.get(63, None))
                advance = 0.0
                pb = [0.0] * 4 # planeBounds
                ab = [0.0] * 4 # atlasBounds

                if g:
                    advance = g.get("advance", 0.0)
                    if "planeBounds" in g:
                        b = g["planeBounds"]
                        pb = [b["left"], b["bottom"], b["right"], b["top"]]
                    if "atlasBounds" in g:
                        b = g["atlasBounds"]
                        ab = [b["left"], b["bottom"], b["right"], b["top"]]

                # Pack 9 floats (36 bytes per glyph)
                f.write(struct.pack("f ffff ffff", advance, *pb, *ab))

            f.write(pixels)
        print(BOLD + "Success!" + BOLD_END)
    finally:
        if os.path.exists(temp_json): os.remove(temp_json)
        if os.path.exists(temp_png): os.remove(temp_png)

def validate_file_extension(filepath, *exts):
    ext = os.path.splitext(filepath)[-1]
    if not ext.lower() in exts:
        raise argparse.ArgumentTypeError(
            f"File does not have the expected extensions: {exts}"
        )
    return filepath

def ttf_file(filepath):
    return validate_file_extension(filepath, ".ttf")

def bin_file(filepath):
    return validate_file_extension(filepath, ".bin")

parser = argparse.ArgumentParser(
        prog="Font to MSDF",
        description=
        """
        Converts .ttf files into a easily readable .bin file.
        Uses msdf_atlas_gen and packs the data
        """
)
parser.add_argument("-f", "--file", help="Path to the ttf file to convert", required=True, type=ttf_file)
parser.add_argument("-o", "--output", help="Path to write the converted font .bin", required=True, type=bin_file)
parser.add_argument("-s", "--size", help="The font size to generate", type=int)
args = parser.parse_args()

build_font(args.file, args.output, args.size)
