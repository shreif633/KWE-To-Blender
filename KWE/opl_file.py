#
# OPL Importer/Exporter: opl_file.py
#
import bpy
import os
import io
import math
import struct
from mathutils import Vector, Quaternion
from .util import BinaryReader, BinaryWriter, KalCRC32

# --- Coordinate System Conversion ---
# Kal Online: Y-Up, Z-Forward (Right-Handed)
# Blender:    Z-Up, -Y-Forward (Right-Handed)
# To convert from Kal to Blender, we need to rotate the entire system by +90 degrees around the X-axis.
#
# Position Conversion:
# kal(x, y, z) -> blender(x, -z, y)
#
# Rotation Conversion:
# The object's rotation quaternion (q_kal) must be composed with the system rotation (R_conv).
# q_blender = R_conv * q_kal

KAL_TO_BLENDER_ROT = Quaternion((1, 0, 0), math.radians(90.0))
BLENDER_TO_KAL_ROT = KAL_TO_BLENDER_ROT.conjugated()

def kal_to_blender_pos(pos_kal):
    """Converts a Kal Online position vector to a Blender position vector."""
    return Vector((pos_kal.x, -pos_kal.z, pos_kal.y))

def blender_to_kal_pos(pos_blender):
    """Converts a Blender position vector back to a Kal Online position vector."""
    return Vector((pos_blender.x, pos_blender.z, -pos_blender.y))

def kal_to_blender_rot(rot_kal_xyzw):
    """Converts a Kal Online rotation (X,Y,Z,W vector) to a Blender quaternion."""
    # mathutils.Quaternion expects (w, x, y, z)
    q_kal = Quaternion((rot_kal_xyzw.w, rot_kal_xyzw.x, rot_kal_xyzw.y, rot_kal_xyzw.z))
    # Apply the coordinate system rotation
    return KAL_TO_BLENDER_ROT @ q_kal

def blender_to_kal_rot_vector(rot_blender_quat):
    """Converts a Blender quaternion back to a Kal Online rotation vector (x, y, z, w)."""
    # Apply the inverse coordinate system rotation
    q_kal = BLENDER_TO_KAL_ROT @ rot_blender_quat
    # Return as a vector in (x, y, z, w) order for writing
    return Vector((q_kal.x, q_kal.y, q_kal.z, q_kal.w))

# --- OPL Import Logic ---
def import_opl(filepath, game_path):
    opl = OPLData()
    if not opl.load(filepath):
        print(f"Failed to load OPL file: {filepath}")
        return

    # Create a new collection for the objects for organization
    collection_name = f"OPL_{os.path.basename(filepath)}"
    collection = bpy.data.collections.new(collection_name)
    bpy.context.scene.collection.children.link(collection)
    
    # Store header data on the collection itself for easy access during export
    collection['opl_header'] = opl.header

    for i, node in enumerate(opl.nodes):
        # Use the object's filename as part of the name, ensuring uniqueness with an index
        obj_name = f"{os.path.basename(node['path']).split('.')[0]}_{i}"
        
        # Create an Empty to represent the object
        empty = bpy.data.objects.new(obj_name, None)
        empty.empty_display_type = 'ARROWS'
        empty.empty_display_size = 2.0
        
        # Apply transforms with coordinate system conversion
        empty.location = kal_to_blender_pos(node['position'])
        empty.rotation_quaternion = kal_to_blender_rot(node['rotation'])
        empty.scale = node['scale']

        # Store original game data as custom properties on the Blender object.
        # This is ESSENTIAL for a successful export.
        empty['game_path'] = node['path']
        
        collection.objects.link(empty)
    
    print(f"Imported {len(opl.nodes)} objects from {os.path.basename(filepath)}")
        
# --- OPL Export Logic ---
def export_opl(filepath, collection, game_path):
    if 'opl_header' not in collection:
        print(f"Export failed: Collection '{collection.name}' is not an imported OPL collection (missing header data).")
        return

    header = collection['opl_header'].to_dict()
    nodes_to_export = []
    
    for obj in collection.objects:
        if 'game_path' not in obj:
            print(f"Skipping object '{obj.name}' as it's missing the 'game_path' property.")
            continue
            
        node = {}
        node['path'] = obj['game_path']
        
        # Convert coordinates back to Kal's system
        node['position'] = blender_to_kal_pos(obj.location)
        node['rotation'] = blender_to_kal_rot_vector(obj.rotation_quaternion)
        node['scale'] = obj.scale
        
        nodes_to_export.append(node)
        
    # Update the object count in the header for export
    header['object_count'] = len(nodes_to_export)
    
    # --- Write OPL file ---
    with open(filepath, 'wb') as f:
        writer = BinaryWriter(f)

        # Write header placeholder (40 zero bytes)
        writer.write_bytes(b'\x00' * 40)
        
        # Write actual header data to correct offsets
        writer.seek(8)
        writer.write_int32(header['map_x'])
        writer.write_int32(header['map_y'])
        writer.seek(32)
        writer.write_int32(header.get('scale_type', 7)) # Default to 7 if not present
        writer.seek(36)
        writer.write_int32(header['object_count'])
        
        writer.seek(40) # Position stream at the start of object data
        
        for node in nodes_to_export:
            # Path (Length-prefixed string)
            writer.write_int32(len(node['path']))
            writer.write_string(node['path'])
            # Position (3 floats)
            writer.write_single(node['position'].x)
            writer.write_single(node['position'].y)
            writer.write_single(node['position'].z)
            # Rotation (4 floats: x, y, z, w)
            writer.write_single(node['rotation'].x)
            writer.write_single(node['rotation'].y)
            writer.write_single(node['rotation'].z)
            writer.write_single(node['rotation'].w)
            # Scale (3 floats)
            writer.write_single(node['scale'].x)
            writer.write_single(node['scale'].y)
            writer.write_single(node['scale'].z)
            
    # --- Final step: Calculate and write the correct CRC for the new file ---
    KalCRC32.crc_file(filepath)
    print(f"Exported {len(nodes_to_export)} objects to {filepath}")


# --- OPL Data Structure Class ---
class OPLData:
    def __init__(self):
        self.header = {}
        self.nodes = []

    def load(self, filepath):
        if not os.path.exists(filepath): return False
        
        with open(filepath, 'rb') as f:
            reader = BinaryReader(f)
            
            # Read Header
            self.header['crc'] = reader.read_int32()
            reader.seek(8)
            self.header['map_x'] = reader.read_int32()
            self.header['map_y'] = reader.read_int32()
            reader.seek(32)
            self.header['scale_type'] = reader.read_byte()
            reader.seek(36)
            self.header['object_count'] = reader.read_int32()
            
            reader.seek(40) # Seek to start of object data
            
            for _ in range(self.header['object_count']):
                try:
                    node = {}
                    path_len = reader.read_int32()
                    # Basic sanity check to prevent reading huge strings from corrupt files
                    if path_len > 1024 or path_len < 1: 
                        print(f"Warning: Invalid path length ({path_len}) found. Stopping OPL read.")
                        break
                    
                    node['path'] = reader.read_string(path_len)
                    
                    px, py, pz = reader.read_single(), reader.read_single(), reader.read_single()
                    node['position'] = Vector((px, py, pz))
                    
                    # Rotation is stored as (x,y,z,w)
                    rx, ry, rz, rw = reader.read_single(), reader.read_single(), reader.read_single(), reader.read_single()
                    node['rotation'] = Vector((rx, ry, rz, rw))
                    
                    # The scale_type logic is unusual. Replicating the Delphi code.
                    if self.header['scale_type'] == 4:
                        s_byte = reader.read_byte()
                        # This implies a uniform scale based on a single byte, followed by a skip.
                        node['scale'] = Vector((float(s_byte), float(s_byte), float(s_byte)))
                        reader.seek(4, 1) # This is Delphi's `Reader.Position := Reader.Position + 4`
                    else:
                        # Standard non-uniform scale from 3 floats
                        sx, sy, sz = reader.read_single(), reader.read_single(), reader.read_single()
                        node['scale'] = Vector((sx, sy, sz))
                    
                    self.nodes.append(node)

                except struct.error:
                    print("Warning: Reached end of file while reading OPL nodes. File may be truncated.")
                    break # Exit loop if we can't read a full node
        return True