#
# KSM Importer: ksm_file.py
#
import bpy
import numpy as np
import os
from .util import BinaryReader

def import_ksm_as_vcolor(filepath, obj):
    """Imports a .ksm file and applies it as a new vertex color layer on the active object."""
    ksm_map = np.zeros((256, 256, 3), dtype=np.uint8)

    with open(filepath, 'rb') as f:
        reader = BinaryReader(f)
        reader.seek(4)
        for y in range(255, -1, -1):
            for x in range(256):
                val1 = reader.read_word()
                val2 = reader.read_word()
                ksm_map[x, y] = values_to_color(val1, val2)
                
    # Use the existing function to apply the color map
    from .kcm_file import apply_vertex_colors
    layer_name = f"KSM_{os.path.basename(filepath)}"
    apply_vertex_colors(obj.data, ksm_map, layer_name)
    print(f"Applied KSM data as vertex color layer '{layer_name}'.")

def values_to_color(val1, val2):
    # Direct translation of TKalServerMap.ValuesToColor
    if val1 == 65535: # Red dominant
        if val2 == 0: return (255, 0, 0)
        if val2 == 1: return (255, 250, 50)
        if val2 == 2: return (255, 200, 100)
        if val2 == 6: return (255, 150, 150)
        if val2 == 4: return (255, 100, 200)
        if val2 == 16: return (255, 50, 250)
        return (255, 0, 0)
    elif val1 == 0: # Green dominant
        if val2 == 0: return (0, 255, 0)
        if val2 == 1: return (250, 255, 50)
        if val2 == 2: return (200, 255, 100)
        if val2 == 6: return (150, 255, 150)
        if val2 == 4: return (100, 255, 200)
        if val2 == 16: return (50, 255, 250)
        return (0, 255, 0)
    else: # Default
        return (255, 0, 0)