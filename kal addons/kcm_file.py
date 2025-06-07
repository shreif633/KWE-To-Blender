#
# KCM Importer/Exporter: kcm_file.py
#
import bpy
import bmesh
import numpy as np
import os
import io
from mathutils import Vector, Color
from .util import BinaryReader, BinaryWriter, KalCRC32
from .gtx_converter import convert_gtx_to_dds

# --- KCM Import Logic ---
def import_kcm(filepath, game_path, terrain_scale, height_scale):
    # --- Step 1: Read the KCM file into memory ---
    kcm = KCMData()
    if not kcm.load(filepath):
        return

    # --- Step 2: Create Blender Mesh ---
    mesh_name = f"KCM_{kcm.header['map_x']}_{kcm.header['map_y']}"
    mesh = bpy.data.meshes.new(name=mesh_name)
    obj = bpy.data.objects.new(mesh_name, mesh)
    
    # Store header data for export
    obj['kcm_header'] = kcm.header

    # Generate vertices from heightmap
    verts = []
    heightmap = kcm.heightmap
    for y in range(257):
        for x in range(257):
            vx = x * terrain_scale
            vy = y * terrain_scale
            vz = heightmap[x, y] * height_scale
            verts.append(Vector((vx, vy, vz)))

    # Generate faces
    faces = []
    for y in range(256):
        for x in range(256):
            v1 = y * 257 + x
            v2 = y * 257 + x + 1
            v3 = (y + 1) * 257 + x + 1
            v4 = (y + 1) * 257 + x
            faces.append((v1, v2, v3, v4))

    mesh.from_pydata(verts, [], faces)

    # --- Step 3: Apply Vertex Colors ---
    apply_vertex_colors(mesh, kcm.colormap, "Colormap")
    
    # --- Step 4: Apply Texture Blend Maps ---
    for i, tex_map in enumerate(kcm.texturemaps):
        if tex_map is not None:
            # We use the alpha channel of the main colormap and other layers
            # Here we just create them as separate greyscale layers for inspection
            apply_blend_map(mesh, tex_map, f"TextureBlend_{i}")
    
    # --- Step 5: Create and Assign Material ---
    create_terrain_material(obj, kcm, game_path)

    # --- Step 6: Link to scene ---
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    mesh.update()
    mesh.validate()

def apply_vertex_colors(mesh, colormap, layer_name="Colormap"):
    vcol_layer = mesh.vertex_colors.new(name=layer_name)
    
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            loop = mesh.loops[loop_index]
            v = mesh.vertices[loop.vertex_index]
            
            # Find the corresponding pixel in the 256x256 map
            # This is an approximation; a more accurate method would be UV-based
            x = int(v.co.x / mesh.vertices[-1].co.x * 255)
            y = int(v.co.y / mesh.vertices[-1].co.y * 255)
            x = max(0, min(255, x))
            y = max(0, min(255, y))

            # Kal's colormap is BGR, Blender is RGB
            b, g, r = colormap[x, y]
            vcol_layer.data[loop_index].color = (r / 255.0, g / 255.0, b / 255.0, 1.0)

def apply_blend_map(mesh, blendmap, layer_name):
    vcol_layer = mesh.vertex_colors.new(name=layer_name)
    
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            loop = mesh.loops[loop_index]
            v = mesh.vertices[loop.vertex_index]
            
            x = int(v.co.x / mesh.vertices[-1].co.x * 255)
            y = int(v.co.y / mesh.vertices[-1].co.y * 255)
            x = max(0, min(255, x))
            y = max(0, min(255, y))

            val = blendmap[x, y] / 255.0
            vcol_layer.data[loop_index].color = (val, val, val, 1.0)
            
# --- Material Creation ---
def create_terrain_material(obj, kcm, game_path):
    env_data = EnvFileData()
    env_path = os.path.join(game_path, "Data", "3dData", "Env", "g_env.env")
    if os.path.exists(env_path):
        env_data.load(env_path)
    
    mat_name = f"MAT_{obj.name}"
    mat = bpy.data.materials.new(name=mat_name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links
    
    # Link main vertex color map to base color
    vc_node = nodes.new('ShaderNodeVertexColor')
    vc_node.layer_name = "Colormap"
    links.new(vc_node.outputs['Color'], bsdf.inputs['Base Color'])
    
    # Create nodes for texture blending (this is a simplified setup)
    # A full setup would involve mixing textures based on blend maps.
    print("Texture List from KCM:", kcm.header['texture_list'])
    print("Texture Names from ENV:", env_data.texture_index_list)

    # Simplified: just create the image nodes, no complex mixing
    y_pos = 0
    for i, tex_idx in enumerate(kcm.header['texture_list']):
        if tex_idx == 255: continue # 0xFF means no texture
        
        if tex_idx < len(env_data.texture_index_list):
            texture_gtx_name = env_data.texture_index_list[tex_idx]
            gtx_path = os.path.join(game_path, "Data", "3dData", "Texture", texture_gtx_name)
            
            # Convert GTX to DDS in a temp location
            temp_dir = os.path.join(bpy.app.tempdir, "kal_textures")
            dds_path = os.path.join(temp_dir, texture_gtx_name.replace('.gtx', '.dds'))

            if not os.path.exists(dds_path):
                 convert_gtx_to_dds(gtx_path, dds_path, game_path)
            
            if os.path.exists(dds_path):
                img = bpy.data.images.load(dds_path)
                tex_node = nodes.new('ShaderNodeTexImage')
                tex_node.image = img
                tex_node.location = (-400, y_pos)
                y_pos -= 300

    obj.data.materials.append(mat)
    
# --- KCM Data Structure Class ---
class KCMData:
    def __init__(self):
        self.header = {}
        self.texturemaps = [None] * 7
        self.heightmap = None
        self.colormap = None
        self.objectmap = None

    def load(self, filepath):
        if not os.path.exists(filepath):
            print(f"File not found: {filepath}")
            return False
            
        with open(filepath, 'rb') as f:
            reader = BinaryReader(f)
            
            # Read Header
            self.header['crc'] = reader.read_int32()
            reader.seek(8)
            self.header['map_x'] = reader.read_int32()
            self.header['map_y'] = reader.read_int32()
            reader.seek(37)
            self.header['texture_list'] = [reader.read_byte() for _ in range(7)]
            
            # Read Texture Maps
            reader.seek(52)
            for i in range(7):
                if self.header['texture_list'][i] != 0xFF:
                    tex_map = np.zeros((256, 256), dtype=np.uint8)
                    for y in range(255, -1, -1):
                        for x in range(256):
                            tex_map[x, y] = reader.read_byte()
                    self.texturemaps[i] = tex_map
                else:
                    break
            
            # Read Height Map (257x257 words)
            self.heightmap = np.zeros((257, 257), dtype=np.uint16)
            for y in range(256, -1, -1):
                for x in range(257):
                    self.heightmap[x, y] = reader.read_word()
                    
            # Read Color Map (256x256 BGR bytes)
            self.colormap = np.zeros((256, 256, 3), dtype=np.uint8)
            for y in range(255, -1, -1):
                for x in range(256):
                    # Delphi: [2]=B, [1]=G, [0]=R. Numpy is fine.
                    b = reader.read_byte()
                    g = reader.read_byte()
                    r = reader.read_byte()
                    self.colormap[x, y] = [b, g, r]

            # Read Object Map (256x256 bytes)
            self.objectmap = np.zeros((256, 256), dtype=np.uint8)
            for y in range(255, -1, -1):
                for x in range(256):
                    self.objectmap[x, y] = reader.read_byte()
        return True


# --- KCM Export Logic ---
def export_kcm(filepath, obj, game_path):
    if not obj or obj.type != 'MESH':
        print("Export failed: Not a mesh object.")
        return

    mesh = obj.data
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bm.verts.ensure_lookup_table()
    
    header = obj['kcm_header'].to_dict()
    
    # --- Extract data from mesh ---
    # Determine scale from vertex positions
    terrain_scale = (bm.verts[-1].co.x / 256.0) if len(bm.verts) > 0 else 10.0
    
    # Heightmap
    heightmap = np.zeros((257, 257), dtype=np.uint16)
    # This assumes vertices are ordered correctly, which they are on import.
    # A more robust solution would check vertex coordinates.
    max_height = 0
    for v in bm.verts:
         if v.co.z > max_height: max_height = v.co.z
    
    height_scale = 1.0
    if max_height > 0:
        # try to find original height scale
        max_height_val = np.max([v.co.z for v in bm.verts])
        height_scale = 65535.0 / max_height_val if max_height_val > 0 else 1.0
    
    for y in range(257):
        for x in range(257):
            idx = y * 257 + x
            heightmap[x, y] = int(bm.verts[idx].co.z * height_scale)

    # Colormap
    colormap = np.zeros((256, 256, 3), dtype=np.uint8)
    vcol_layer = mesh.vertex_colors.get("Colormap")
    if vcol_layer:
        # Sample colors at grid points
        for y_grid in range(256):
            for x_grid in range(256):
                # This is a simplification; should ideally sample from the mesh surface
                loop_idx = (y_grid * 256 + x_grid) * 4 # Approx
                if loop_idx < len(vcol_layer.data):
                    c = vcol_layer.data[loop_idx].color
                    colormap[x_grid, y_grid] = [int(c.b*255), int(c.g*255), int(c.r*255)] # Write as BGR
    
    # Texturemaps (blend maps)
    texturemaps = [None] * 7
    for i in range(7):
        blend_layer = mesh.vertex_colors.get(f"TextureBlend_{i}")
        if blend_layer:
            tmap = np.zeros((256, 256), dtype=np.uint8)
            for y_grid in range(256):
                for x_grid in range(256):
                    loop_idx = (y_grid * 256 + x_grid) * 4
                    if loop_idx < len(blend_layer.data):
                        c = blend_layer.data[loop_idx].color
                        tmap[x_grid, y_grid] = int(c.r * 255) # Assuming greyscale
            texturemaps[i] = tmap

    # Objectmap (not modified, just use imported one for now)
    objectmap = np.zeros((256, 256), dtype=np.uint8) # Placeholder
    
    bm.free()

    # --- Write KCM file ---
    with open(filepath, 'wb') as f:
        writer = BinaryWriter(f)
        
        # Write header placeholder (52 zero bytes)
        writer.write_bytes(b'\x00' * 52)
        
        # Write header data
        writer.seek(8)
        writer.write_int32(header['map_x'])
        writer.write_int32(header['map_y'])
        writer.seek(0x20)
        writer.write_byte(0x07) # BS?
        writer.seek(0x25)
        for tex_idx in header['texture_list']:
            writer.write_byte(tex_idx)
        for i in range(8): # More BS?
             writer.write_byte(i)
        
        writer.seek(52)
        
        # Write Texture Maps
        for i in range(7):
            if header['texture_list'][i] != 0xFF:
                tmap = texturemaps[i]
                if tmap is not None:
                    for y in range(255, -1, -1):
                        for x in range(256):
                            writer.write_byte(int(tmap[x, y]))
                else: # Write dummy data if map is missing
                    writer.write_bytes(b'\x00' * (256*256))
            else:
                break

        # Write Height Map
        for y in range(256, -1, -1):
            for x in range(257):
                writer.write_word(int(heightmap[x, y]))
                
        # Write Color Map (BGR)
        for y in range(255, -1, -1):
            for x in range(256):
                b, g, r = colormap[x, y]
                writer.write_byte(int(b))
                writer.write_byte(int(g))
                writer.write_byte(int(r))
                
        # Write Object Map
        for y in range(255, -1, -1):
            for x in range(256):
                writer.write_byte(int(objectmap[x,y]))

    # --- Final step: Calculate and write CRC ---
    KalCRC32.crc_file(filepath)


# --- ENV File Parser (needed for textures) ---
class EnvFileData:
    def __init__(self):
        self.grass_list = []
        self.texture_index_list = []

    def load(self, filepath):
        # This is a direct translation of the very specific Delphi file parsing
        with open(filepath, 'rb') as f:
            reader = BinaryReader(f)
            reader.seek(0x28)
            
            # Read grass list
            i = 0
            while i < 2:
                index = reader.read_int32()
                if index == 1:
                    i += 1
                    if i == 2: break
                
                str_len = reader.read_int32()
                text = reader.read_string(str_len)
                self.grass_list.append(text + '.gb')

            # Find start of texture list by searching for "b_001"
            f.seek(0)
            content = f.read()
            try:
                start_pos = content.find(b'b_001')
                if start_pos == -1: return # Not found
                
                reader.seek(start_pos - 4) # Go back to read the length
                
                while reader.tell() < len(content) - 1:
                    str_len = reader.read_int32()
                    if str_len > 100 or str_len < 1: break # Sanity check
                    
                    text = reader.read_string(str_len)
                    self.texture_index_list.append(text + '.gtx')
                    
                    if reader.tell() + 8 > len(content): break # Avoid reading past EOF
                    reader.read_int32() # Skip two integers
                    reader.read_int32()
            except struct.error:
                # Reached end of file
                pass