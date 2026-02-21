import struct, subprocess, os, argparse, math
from PIL import Image

BOLD = "\033[1m"
BOLD_END = "\033[0m"

def next_power_of_two(x):
    return 1 if x == 0 else 2**(x-1).bit_length()


def build_icon_atlas(svg_dir, output_bin, icon_size, px_range=4):
    print(BOLD + f"Scanning directory: {svg_dir}..." + BOLD_END)
    svg_files = [f for f in os.listdir(svg_dir) if f.endswith('.svg')]
    if not svg_files:
        print("Error: No SVG files found in directory.")
        return

    num_icons = len(svg_files)
    cols = math.ceil(math.sqrt(num_icons))
    rows = math.ceil(num_icons / cols)
    atlas_width = next_power_of_two(cols * icon_size)
    atlas_height = next_power_of_two(rows * icon_size)

    print(BOLD + f"Found {num_icons}. Building a {atlas_width}x{atlas_height} atlas..." + BOLD_END)
    
    atlas_img = Image.new("RGB", (atlas_width, atlas_height), (0,0,0))
    atlas_regions = {}

    for index, svg_file in enumerate(svg_files):
        icon_name = os.path.splitext(svg_file)[0]
        svg_path = os.path.join(svg_dir, svg_file)
        temp_png = os.path.join(svg_dir, f"temp_{index}.png")
        print(f"  -> Rendering {icon_name}.svg ...")

        cmd = [
            "msdfgen", "msdf",
            "-svg", svg_path,
            "-o", temp_png,
            "-size", str(icon_size), str(icon_size),
            "-pxrange", str(px_range),
            "-autoframe"
        ]

        try:
            subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)

            icon_img = Image.open(temp_png).convert("RGB")

            col = index % cols
            row = index // cols
            x = col * icon_size
            y = row * icon_size

            atlas_img.paste(icon_img, (x,y))

            atlas_regions[icon_name] = {
                "x": x,
                "y": y,
                "w": icon_size,
                "h": icon_size
            }

        except subprocess.CalledProcessError:
            print(f"    [!] Failed to generate {svg_file}")
        finally:
            if os.path.exists(temp_png):
                os.remove(temp_png)
    
    print(BOLD + f"\nPacking binary into {output_bin}..." + BOLD_END)
    pixels = atlas_img.tobytes()
    num_regions = len(atlas_regions)

    with open(output_bin, "wb") as f:
        header = struct.pack("<4s I I I f I",
                             b'ATLS',
                             atlas_width,
                             atlas_height,
                             3, #RGB
                             float(px_range),
                             num_regions)
        f.write(header)
        
        # Write UV region metadata
        for name, rect in atlas_regions.items():
            name_bytes = name.encode('utf-8')[:31].ljust(32, b'\0')

            region_data = struct.pack("<32s I I I I",
                                      name_bytes,
                                      rect["x"],
                                      rect["y"],
                                      rect["w"],
                                      rect["h"])
            f.write(region_data)
        
        # Write raw pixel data
        f.write(pixels)

    print(BOLD + f"Done. Outputted {output_bin}." + BOLD_END)

def validate_file_extension(filepath, *exts):
    ext = os.path.splitext(filepath)[-1]
    if not ext.lower() in exts:
        raise argparse.ArgumentTypeError(
            f"File does not have the expected extensions: {exts}"
        )
    return filepath

def validate_directory(dirpath):
    if not os.path.isdir(dirpath):
        raise argparse.ArgumentTypeError(f"Directory does not exist: {dirpath}")
    return dirpath

def svg_file(filepath):
    return validate_file_extension(filepath, ".svg")

def bin_file(filepath):
    return validate_file_extension(filepath, ".bin")

parser = argparse.ArgumentParser(
        prog="SVGs to MSDF atlas",
        description=
        """
        Takes a directory of .svg files and packs it into a single MSDF .bin Atlas
        Uses msdf_atlas_gen.
        Packs UV data into a byte header.
        """
)
parser.add_argument("-d", "--dir", help="Directory containing .svg files", required=True, type=validate_directory)
parser.add_argument("-o", "--output", help="Path to write the converted svg .bin", required=True, type=bin_file)
parser.add_argument("-s", "--size", help="Size of each icon in pixels.", type=int, required=True)
parser.add_argument("-r", "--pxrange", help="Size of each icon in pixels.", type=int)
args = parser.parse_args()

build_icon_atlas(args.dir, args.output, args.size, args.pxrange if args.pxrange else 4)
