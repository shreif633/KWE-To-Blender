#
# KCM Importer/Exporter: kcm_file.py
#
import bpy
import bmesh
import numpy as np
import os
import struct
from mathutils import Vector
from .util import BinaryReader, BinaryWriter, KalCRC32
from .gtx_converter import convert_gtx_to_dds

# --- Data Structures ---
class KCMData:
    def __init__(self):
        self.header, self.texturemaps, self.heightmap, self.colormap, self.objectmap = {}, [None] * 7, None, None, None
    def load(self, filepath):
        if not os.path.exists(filepath): return False
        with open(filepath, 'rb') as f:
            r = BinaryReader(f)
            self.header['crc'] = r.read_int32()
            r.seek(8); self.header['map_x'], self.header['map_y'] = r.read_int32(), r.read_int32()
            r.seek(37); self.header['texture_list'] = [r.read_byte() for _ in range(7)]
            r.seek(52)
            for i in range(7):
                if self.header['texture_list'][i] != 0xFF:
                    tex_map = np.zeros((256, 256), dtype=np.uint8)
                    for y in range(255, -1, -1):
                        for x in range(256): tex_map[x, y] = r.read_byte()
                    self.texturemaps[i] = tex_map
                else: break
            self.heightmap = np.zeros((257, 257), dtype=np.uint16)
            for y in range(256, -1, -1):
                for x in range(257): self.heightmap[x, y] = r.read_word()
            self.colormap = np.zeros((256, 256, 3), dtype=np.uint8)
            for y in range(255, -1, -1):
                for x in range(256): self.colormap[x, y] = [r.read_byte(), r.read_byte(), r.read_byte()] # BGR
            self.objectmap = np.zeros((256, 256), dtype=np.uint8)
            for y in range(255, -1, -1):
                for x in range(256): self.objectmap[x, y] = r.read_byte()
        return True

class EnvFileData:
    def __init__(self): self.grass_list, self.texture_index_list = [], []
    def load(self, filepath):
        if not os.path.exists(filepath): return
        with open(filepath, 'rb') as f:
            r = BinaryReader(f)
            r.seek(0x28)
            i = 0
            while i < 2:
                try:
                    index = r.read_int32()
                    if index == 1:
                        i += 1
                        if i == 2: break
                    str_len = r.read_int32()
                    self.grass_list.append(r.read_string(str_len) + '.gb')
                except struct.error: break
            f.seek(0)
            content = f.read()
            try:
                start_pos = content.find(b'b_001')
                if start_pos == -1: return
                r.seek(start_pos - 4)
                while r.tell() < len(content) - 1:
                    str_len = r.read_int32()
                    if str_len > 100 or str_len < 1: break
                    self.texture_index_list.append(r.read_string(str_len) + '.gtx')
                    if r.tell() + 8 > len(content): break
                    r.read_int32(); r.read_int32()
            except struct.error: pass

# --- KCM Import Logic ---
def import_kcm(filepath, game_path, terrain_grid_scale, height_scale):
    kcm = KCMData()
    if not kcm.load(filepath): return

    mesh_name = f"KCM_{kcm.header['map_x']}_{kcm.header['map_y']}"
    mesh = bpy.data.meshes.new(name=mesh_name)
    obj = bpy.data.objects.new(mesh_name, mesh)
    obj['kcm_header'] = kcm.header
    obj['kcm_import_grid_scale'] = terrain_grid_scale
    obj['kcm_import_height_scale'] = height_scale

    verts = [Vector((x * terrain_grid_scale, y * terrain_grid_scale, kcm.heightmap[x, y] * height_scale)) for y in range(257) for x in range(257)]
    faces = [(y * 257 + x, y * 257 + x + 1, (y + 1) * 257 + x + 1, (y + 1) * 257 + x) for y in range(256) for x in range(256)]
    mesh.from_pydata(verts, [], faces)
    
    bm = bmesh.new(); bm.from_mesh(mesh)
    uv_layer = bm.loops.layers.uv.verify()
    for face in bm.faces:
        for loop in face.loops: loop[uv_layer].uv = loop.vert.co.xy / (256 * terrain_grid_scale)
    bm.to_mesh(mesh); bm.free()

    apply_greyscale_map(mesh, kcm.colormap, "Colormap_RGB")
    for i, tex_map in enumerate(kcm.texturemaps):
        if tex_map is not None: apply_greyscale_map(mesh, tex_map, f"BlendMap_{i}")
    
    create_terrain_material(obj, kcm, game_path)

    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    mesh.update(); mesh.validate()

def apply_greyscale_map(mesh, map_data, layer_name):
    if mesh.vertex_colors.get(layer_name): vcol_layer = mesh.vertex_colors[layer_name]
    else: vcol_layer = mesh.vertex_colors.new(name=layer_name)
    is_greyscale = len(map_data.shape) == 2
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            uv = mesh.uv_layers.active.data[loop_index].uv
            x, y = int(uv.x * 255), int(uv.y * 255)
            x, y = max(0, min(255, x)), max(0, min(255, y))
            if is_greyscale:
                val = map_data[x, y] / 255.0
                vcol_layer.data[loop_index].color = (val, val, val, 1.0)
            else:
                b, g, r = map_data[x, y]
                vcol_layer.data[loop_index].color = (r / 255.0, g / 255.0, b / 255.0, 1.0)

def create_terrain_material(obj, kcm, game_path):
    env_data = EnvFileData()
    env_path = os.path.join(game_path, "Data", "3dData", "Env", "g_env.env")
    env_data.load(env_path)
    mat = bpy.data.materials.new(name=f"MAT_{obj.name}")
    mat.use_nodes = True
    obj.data.materials.append(mat)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()

    bsdf, output = nodes.new("ShaderNodeBsdfPrincipled"), nodes.new("ShaderNodeOutputMaterial")
    bsdf.location, output.location = (400, 0), (600, 0)
    tex_coord = nodes.new("ShaderNodeTexCoord"); tex_coord.location = (-1200, 400)
    
    last_color_socket, node_x, node_y = None, -800, 500
    for i, tex_idx in enumerate(kcm.header['texture_list']):
        if tex_idx == 255: continue
        img = None
        if tex_idx < len(env_data.texture_index_list):
            gtx_name = env_data.texture_index_list[tex_idx]
            gtx_path = os.path.join(game_path, "Data", "3dData", "Texture", gtx_name)
            dds_path = os.path.join(bpy.app.tempdir, "kal_textures", gtx_name.replace('.gtx', '.dds'))
            if not os.path.exists(dds_path): convert_gtx_to_dds(gtx_path, dds_path, game_path)
            if os.path.exists(dds_path): img = bpy.data.images.load(dds_path)
        
        tex_node = nodes.new('ShaderNodeTexImage'); tex_node.name = f"Texture_{i}"; tex_node.label = f"Texture_{i}"
        tex_node.image, tex_node.location = img, (node_x, node_y)
        links.new(tex_coord.outputs['UV'], tex_node.inputs['Vector'])

        if i == 0:
            last_color_socket = tex_node.outputs['Color']
        else:
            blend_map_node = nodes.new('ShaderNodeVertexColor'); blend_map_node.layer_name = f"BlendMap_{i-1}"
            blend_map_node.location = (node_x, node_y - 150)
            mix_node = nodes.new('ShaderNodeMixRGB'); mix_node.location = (node_x + 400, node_y - 50)
            links.new(last_color_socket, mix_node.inputs['Color1'])
            links.new(tex_node.outputs['Color'], mix_node.inputs['Color2'])
            links.new(blend_map_node.outputs['Color'], mix_node.inputs['Fac'])
            last_color_socket = mix_node.outputs['Color']
        node_y -= 350
        
    colormap_node = nodes.new('ShaderNodeVertexColor'); colormap_node.layer_name = "Colormap_RGB"; colormap_node.location = (-200, -200)
    final_mix = nodes.new('ShaderNodeMixRGB'); final_mix.blend_type = 'MULTIPLY'; final_mix.inputs['Fac'].default_value = 1.0; final_mix.location = (200, 0)
    if last_color_socket: links.new(last_color_socket, final_mix.inputs['Color1'])
    links.new(colormap_node.outputs['Color'], final_mix.inputs['Color2'])
    links.new(final_mix.outputs['Color'], bsdf.inputs['Base Color'])
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

def setup_texture_paint_slots(obj):
    if not obj.data.materials or not obj.data.materials[0]: return
    mat = obj.data.materials[0]; mat.paint_settings.image_paint_source = 'VERTEX_COLOR'
    for i in range(len(mat.texture_paint_slots)): mat.texture_paint_slots.clear()
    for i in range(7):
        layer_name = f"BlendMap_{i}"
        if layer_name in obj.data.vertex_colors:
            slot = mat.texture_paint_slots.add()
            slot.uv_layer = obj.data.uv_layers.active.name
            slot.vertex_color_layer = layer_name

# --- KCM Export Logic ---
def export_kcm(filepath, obj, game_path):
    if not obj or obj.type != 'MESH' or 'kcm_header' not in obj: return
    mesh, header = obj.data, obj['kcm_header'].to_dict()
    grid_scale, height_scale = obj.get('kcm_import_grid_scale', 1.0), obj.get('kcm_import_height_scale', 0.005)
    if height_scale == 0: return

    heightmap = np.zeros((257, 257), dtype=np.uint16)
    if len(mesh.vertices) < 66049: return
    for y in range(257):
        for x in range(257):
            idx = y * 257 + x
            h = int(mesh.vertices[idx].co.z / height_scale)
            heightmap[x, y] = max(0, min(65535, h))

    map_size = 256
    colormap = np.zeros((map_size, map_size, 3), dtype=np.uint8)
    vcol_rgb = mesh.vertex_colors.get("Colormap_RGB")
    if vcol_rgb and mesh.uv_layers.active:
        for p in mesh.polygons:
            for l_idx in p.loop_indices:
                uv = mesh.uv_layers.active.data[l_idx].uv
                x, y = int(uv.x * (map_size-1)), int(uv.y * (map_size-1))
                c = vcol_rgb.data[l_idx].color
                colormap[x, y] = [int(c.b*255), int(c.g*255), int(c.r*255)] # BGR

    texturemaps = [None] * 7
    for i in range(7):
        layer_name = f"BlendMap_{i}"
        if mesh.vertex_colors.get(layer_name):
            tmap = np.zeros((map_size, map_size), dtype=np.uint8)
            vcol = mesh.vertex_colors[layer_name]
            if mesh.uv_layers.active:
                for p in mesh.polygons:
                    for l_idx in p.loop_indices:
                        uv = mesh.uv_layers.active.data[l_idx].uv
                        x, y = int(uv.x * (map_size-1)), int(uv.y * (map_size-1))
                        tmap[x, y] = int(vcol.data[l_idx].color.r * 255)
            texturemaps[i] = tmap

    objectmap = np.zeros((map_size, map_size), dtype=np.uint8)

    with open(filepath, 'wb') as f:
        w = BinaryWriter(f)
        w.write_bytes(b'\x00' * 52); w.seek(8)
        w.write_int32(header['map_x']); w.write_int32(header['map_y'])
        w.seek(0x20); w.write_byte(0x07); w.seek(0x25)
        for tex_idx in header['texture_list']: w.write_byte(tex_idx)
        for i in range(8): w.write_byte(i)
        w.seek(52)
        for i in range(7):
            if header['texture_list'][i] != 0xFF:
                tmap = texturemaps[i]
                if tmap is not None:
                    for y in range(map_size-1, -1, -1):
                        for x in range(map_size): w.write_byte(int(tmap[x, y]))
                else: w.write_bytes(b'\x00' * (map_size*map_size))
            else: break
        for y in range(256, -1, -1):
            for x in range(257): w.write_word(int(heightmap[x, y]))
        for y in range(map_size-1, -1, -1):
            for x in range(map_size): w.write_byte(int(colormap[x, y][0])); w.write_byte(int(colormap[x, y][1])); w.write_byte(int(colormap[x, y][2]))
        for y in range(map_size-1, -1, -1):
            for x in range(map_size): w.write_byte(int(objectmap[x,y]))

    KalCRC32.crc_file(filepath)
    print(f"Exported KCM to {filepath}")