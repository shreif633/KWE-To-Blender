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


# --- Path Helper Functions ---
def get_addon_preferences():
    """Get addon preferences with path settings"""
    try:
        addon_name = __name__.split('.')[0]
        prefs = bpy.context.preferences.addons[addon_name].preferences
        return prefs
    except:
        return None


def get_env_file_path(game_path=None):
    """Get the ENV file path from preferences or game path"""
    prefs = get_addon_preferences()

    if prefs and prefs.env_file_path and os.path.exists(prefs.env_file_path):
        return prefs.env_file_path

    if not game_path and prefs:
        game_path = prefs.game_path

    if game_path:
        default_path = os.path.join(game_path, "Data", "3dData", "Env", "g_env.env")
        if os.path.exists(default_path):
            return default_path

    return None


def get_texture_folder_path(game_path=None):
    """Get the texture folder path from preferences or game path"""
    prefs = get_addon_preferences()

    if prefs and prefs.texture_folder_path and os.path.exists(prefs.texture_folder_path):
        return prefs.texture_folder_path

    if not game_path and prefs:
        game_path = prefs.game_path

    if game_path:
        default_path = os.path.join(game_path, "Data", "3dData", "Texture")
        if os.path.exists(default_path):
            return default_path

    return None


def get_dds_output_path():
    """Get the DDS output folder path from preferences or temp folder"""
    prefs = get_addon_preferences()

    if prefs and prefs.dds_output_path:
        os.makedirs(prefs.dds_output_path, exist_ok=True)
        return prefs.dds_output_path

    # Default to temp folder
    temp_dir = os.path.join(bpy.app.tempdir, "kal_textures")
    os.makedirs(temp_dir, exist_ok=True)
    return temp_dir


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
    def __init__(self):
        self.grass_list, self.texture_index_list = [], []

    def load(self, filepath):
        if not os.path.exists(filepath):
            print(f"ENV file not found: {filepath}")
            return

        print(f"Loading ENV file: {filepath}")

        try:
            with open(filepath, 'rb') as f:
                # Read grass types first (converted from Delphi 7 code)
                f.seek(0x28)  # Seek to position 0x28 (40 decimal)

                i = 0
                while i < 2:
                    try:
                        # Read index (4 bytes)
                        index_bytes = f.read(4)
                        if len(index_bytes) < 4:
                            break
                        index = struct.unpack('<I', index_bytes)[0]

                        if index == 1:
                            i += 1
                            if i == 2:
                                break

                        # Read string length (4 bytes)
                        str_len_bytes = f.read(4)
                        if len(str_len_bytes) < 4:
                            break
                        str_len = struct.unpack('<I', str_len_bytes)[0]

                        if str_len > 0 and str_len < 100:
                            # Read string
                            text_bytes = f.read(str_len)
                            if len(text_bytes) == str_len:
                                text = text_bytes.decode('ascii', errors='ignore')
                                self.grass_list.append(f"Grass type {index} = {text}.gb")
                                print(f"Found grass type {index}: {text}.gb")

                    except (struct.error, UnicodeDecodeError) as e:
                        print(f"Error reading grass data: {e}")
                        break

                # Read texture list (converted from Delphi 7 code)
                # Find the offset of the first texture by searching for 'b_001'
                f.seek(0)
                file_content = f.read()
                file_size = len(file_content)

                # Search for 'b_001' marker
                texture_start_pos = None
                for x in range(file_size - 5):
                    if file_content[x:x+5] == b'b_001':
                        texture_start_pos = x
                        break

                if texture_start_pos is None:
                    print("Could not find texture list start marker 'b_001' in ENV file")
                    return

                print(f"Found texture list marker at position: {texture_start_pos}")

                # Move 4 bytes back from the marker position
                f.seek(texture_start_pos - 4)

                # Read textures until end of file
                texture_count = 0
                while f.tell() < file_size - 1:
                    try:
                        texture_count += 1

                        # Read string length (4 bytes)
                        str_len_bytes = f.read(4)
                        if len(str_len_bytes) < 4:
                            break
                        str_len = struct.unpack('<I', str_len_bytes)[0]

                        if str_len > 100 or str_len < 1:
                            break

                        # Read texture name
                        text_bytes = f.read(str_len)
                        if len(text_bytes) != str_len:
                            break

                        text = text_bytes.decode('ascii', errors='ignore')
                        self.texture_index_list.append(text + '.gtx')

                        # Skip two integers (8 bytes total)
                        skip_bytes = f.read(8)
                        if len(skip_bytes) < 8:
                            break

                    except (struct.error, UnicodeDecodeError) as e:
                        print(f"Error reading texture {texture_count}: {e}")
                        break

                print(f"Successfully loaded ENV file:")
                print(f"  - {len(self.grass_list)} grass types")
                print(f"  - {len(self.texture_index_list)} textures")

                # Show first few textures for verification
                if self.texture_index_list:
                    print("First 10 textures:")
                    for i, tex in enumerate(self.texture_index_list[:10]):
                        print(f"  {i}: {tex}")

        except Exception as e:
            print(f"Failed to load ENV file {filepath}: {e}")
            import traceback
            traceback.print_exc()

# --- KCM Import Logic ---
def import_kcm(filepath, game_path, terrain_grid_scale, height_scale):
    kcm = KCMData()
    if not kcm.load(filepath): return

    # Reduce terrain size to 5% of original size
    terrain_grid_scale = terrain_grid_scale * 0.05
    height_scale = height_scale * 0.05

    # Get map coordinates
    map_x = kcm.header['map_x']
    map_y = kcm.header['map_y']

    mesh_name = f"KCM_{map_x}_{map_y}"
    mesh = bpy.data.meshes.new(name=mesh_name)
    obj = bpy.data.objects.new(mesh_name, mesh)
    obj['kcm_header'] = kcm.header
    obj['kcm_import_grid_scale'] = terrain_grid_scale
    obj['kcm_import_height_scale'] = height_scale

    # Calculate terrain tile size (256 * terrain_grid_scale)
    tile_size = 256 * terrain_grid_scale

    # Position the terrain based on its X,Y coordinates
    # Each terrain tile is 256x256 units, so offset by tile_size * coordinate
    terrain_offset_x = map_x * tile_size
    terrain_offset_y = map_y * tile_size

    print(f"Importing KCM {map_x},{map_y} at world position ({terrain_offset_x:.2f}, {terrain_offset_y:.2f})")

    # Create vertices with proper world positioning
    verts = []
    for y in range(257):
        for x in range(257):
            world_x = (x * terrain_grid_scale) + terrain_offset_x
            world_y = (y * terrain_grid_scale) + terrain_offset_y
            world_z = kcm.heightmap[x, y] * height_scale
            verts.append(Vector((world_x, world_y, world_z)))

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

    # Use path helpers to get correct ENV file path
    env_path = get_env_file_path(game_path)
    if not env_path:
        print(f"ENV file not found. Check paths in addon preferences.")
        return

    env_data.load(env_path)

    # Store texture info in object for later editing
    obj['kcm_textures'] = {
        'texture_list': kcm.header['texture_list'],
        'env_textures': env_data.texture_index_list,
        'game_path': game_path
    }

    mat = bpy.data.materials.new(name=f"MAT_{obj.name}")
    mat.use_nodes = True
    obj.data.materials.append(mat)
    nodes, links = mat.node_tree.nodes, mat.node_tree.links
    nodes.clear()

    # Create main nodes
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    output = nodes.new("ShaderNodeOutputMaterial")
    bsdf.location, output.location = (800, 0), (1000, 0)

    # Texture coordinate node
    tex_coord = nodes.new("ShaderNodeTexCoord")
    tex_coord.location = (-1400, 400)

    # Mapping node for texture scaling
    mapping = nodes.new("ShaderNodeMapping")
    mapping.location = (-1200, 400)
    mapping.inputs['Scale'].default_value = (1.0, 1.0, 1.0)  # Can be adjusted for texture tiling
    links.new(tex_coord.outputs['UV'], mapping.inputs['Vector'])

    # Load and setup texture nodes
    texture_nodes = []
    last_color_socket = None
    node_x, node_y = -800, 600

    for i, tex_idx in enumerate(kcm.header['texture_list']):
        if tex_idx == 255: continue

        # Load texture image
        img = None
        texture_name = f"Unknown_Texture_{tex_idx}"

        if tex_idx < len(env_data.texture_index_list):
            gtx_name = env_data.texture_index_list[tex_idx]
            texture_name = gtx_name.replace('.gtx', '')

            # Use path helpers to get correct texture folder and DDS output paths
            texture_folder = get_texture_folder_path(game_path)
            if not texture_folder:
                print(f"Texture folder not found. Check paths in addon preferences.")
                continue

            gtx_path = os.path.join(texture_folder, gtx_name)
            dds_output_dir = get_dds_output_path()
            dds_path = os.path.join(dds_output_dir, gtx_name.replace('.gtx', '.dds'))

            # Convert GTX to DDS if needed
            if not os.path.exists(dds_path):
                if convert_gtx_to_dds(gtx_path, dds_path, game_path):
                    print(f"Converted {gtx_name} to DDS")
                else:
                    print(f"Failed to convert {gtx_name}")

            # Load the DDS image
            if os.path.exists(dds_path):
                try:
                    img = bpy.data.images.load(dds_path)
                    img.name = f"KCM_Tex_{i}_{texture_name}"
                    # Ensure texture displays correctly in viewport
                    img.colorspace_settings.name = 'sRGB'
                except Exception as e:
                    print(f"Failed to load texture {dds_path}: {e}")

        # Create texture node
        tex_node = nodes.new('ShaderNodeTexImage')
        tex_node.name = f"Texture_{i}"
        tex_node.label = f"Tex {i}: {texture_name}"
        tex_node.image = img
        tex_node.location = (node_x, node_y)

        # Connect UV mapping
        links.new(mapping.outputs['Vector'], tex_node.inputs['Vector'])

        # Set texture interpolation for better quality
        if img:
            tex_node.interpolation = 'Linear'

        texture_nodes.append(tex_node)

        # Handle texture blending
        if i == 0:
            # First texture is the base
            last_color_socket = tex_node.outputs['Color']
        else:
            # Create blend map node for this texture
            blend_map_node = nodes.new('ShaderNodeVertexColor')
            blend_map_node.layer_name = f"BlendMap_{i-1}"
            blend_map_node.location = (node_x, node_y - 200)

            # Create mix node for blending
            mix_node = nodes.new('ShaderNodeMixRGB')
            mix_node.blend_type = 'MIX'
            mix_node.location = (node_x + 300, node_y - 100)

            # Connect the blend
            links.new(last_color_socket, mix_node.inputs['Color1'])
            links.new(tex_node.outputs['Color'], mix_node.inputs['Color2'])
            links.new(blend_map_node.outputs['Color'], mix_node.inputs['Fac'])

            last_color_socket = mix_node.outputs['Color']

        node_y -= 400

    # Apply colormap multiplication
    colormap_node = nodes.new('ShaderNodeVertexColor')
    colormap_node.layer_name = "Colormap_RGB"
    colormap_node.location = (200, -300)

    # Final color mixing
    final_mix = nodes.new('ShaderNodeMixRGB')
    final_mix.blend_type = 'MULTIPLY'
    final_mix.inputs['Fac'].default_value = 1.0
    final_mix.location = (500, 0)

    if last_color_socket:
        links.new(last_color_socket, final_mix.inputs['Color1'])
    links.new(colormap_node.outputs['Color'], final_mix.inputs['Color2'])
    links.new(final_mix.outputs['Color'], bsdf.inputs['Base Color'])

    # Connect to output
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Set material properties for better viewport display
    mat.use_backface_culling = False
    mat.show_transparent_back = False

    print(f"Created material with {len(texture_nodes)} textures for {obj.name}")

    # Set viewport shading to show textures properly
    setup_viewport_for_textures()


def setup_viewport_for_textures():
    """Configure viewport settings for optimal texture display"""
    try:
        # Get the current 3D viewport
        for area in bpy.context.screen.areas:
            if area.type == 'VIEW_3D':
                for space in area.spaces:
                    if space.type == 'VIEW_3D':
                        # Set to Material Preview mode for best texture display
                        space.shading.type = 'MATERIAL_PREVIEW'
                        # Enable backface culling for better performance
                        space.shading.show_backface_culling = True
                        # Use high quality viewport rendering
                        space.shading.use_scene_lights_render = False
                        space.shading.use_scene_world_render = False
                        break
                break
        print("Viewport configured for texture display")
    except Exception as e:
        print(f"Could not configure viewport: {e}")


def get_texture_info(obj):
    """Get information about textures used in a KCM terrain object"""
    if not obj or 'kcm_textures' not in obj:
        return None

    texture_data = obj.get('kcm_textures', {})
    texture_list = texture_data.get('texture_list', [])
    env_textures = texture_data.get('env_textures', [])

    info = {
        'terrain_name': obj.name,
        'texture_count': 0,
        'textures': []
    }

    if obj.data.materials:
        mat = obj.data.materials[0]

        for i, tex_idx in enumerate(texture_list):
            if tex_idx == 255:
                continue

            tex_info = {
                'index': i,
                'tex_idx': tex_idx,
                'name': 'Unknown',
                'has_image': False,
                'node_name': f"Texture_{i}"
            }

            if tex_idx < len(env_textures):
                tex_info['name'] = env_textures[tex_idx].replace('.gtx', '')

            # Check if texture node exists and has image
            tex_node = mat.node_tree.nodes.get(f"Texture_{i}")
            if tex_node and tex_node.image:
                tex_info['has_image'] = True
                tex_info['image_name'] = tex_node.image.name
                tex_info['image_size'] = tex_node.image.size[:]

            info['textures'].append(tex_info)
            info['texture_count'] += 1

    return info


def debug_texture_loading(game_path, texture_indices):
    """Debug function to test texture loading without importing a full terrain"""
    print("=== Texture Loading Debug ===")

    # Load ENV file
    env_data = EnvFileData()
    env_path = os.path.join(game_path, "Data", "3dData", "Env", "g_env.env")
    env_data.load(env_path)

    print(f"ENV file loaded: {len(env_data.texture_index_list)} textures found")

    # Test texture conversion for specified indices
    temp_dir = os.path.join(bpy.app.tempdir, "kal_textures")
    os.makedirs(temp_dir, exist_ok=True)

    for tex_idx in texture_indices:
        if tex_idx < len(env_data.texture_index_list):
            gtx_name = env_data.texture_index_list[tex_idx]
            gtx_path = os.path.join(game_path, "Data", "3dData", "Texture", gtx_name)
            dds_path = os.path.join(temp_dir, gtx_name.replace('.gtx', '.dds'))

            print(f"\nTesting texture {tex_idx}: {gtx_name}")
            print(f"  GTX path: {gtx_path}")
            print(f"  GTX exists: {os.path.exists(gtx_path)}")
            print(f"  DDS path: {dds_path}")

            if os.path.exists(gtx_path):
                if convert_gtx_to_dds(gtx_path, dds_path, game_path):
                    print(f"  ✓ Conversion successful")
                    if os.path.exists(dds_path):
                        file_size = os.path.getsize(dds_path)
                        print(f"  DDS file size: {file_size} bytes")
                    else:
                        print(f"  ✗ DDS file not created")
                else:
                    print(f"  ✗ Conversion failed")
            else:
                print(f"  ✗ GTX file not found")
        else:
            print(f"\nTexture index {tex_idx} out of range (max: {len(env_data.texture_index_list)-1})")


def list_available_textures(game_path, max_count=20):
    """List available textures in the game directory"""
    print("=== Available Textures ===")

    env_data = EnvFileData()
    env_path = os.path.join(game_path, "Data", "3dData", "Env", "g_env.env")
    env_data.load(env_path)

    print(f"Found {len(env_data.texture_index_list)} textures in ENV file:")

    for i, texture_name in enumerate(env_data.texture_index_list[:max_count]):
        gtx_path = os.path.join(game_path, "Data", "3dData", "Texture", texture_name)
        exists = "✓" if os.path.exists(gtx_path) else "✗"
        print(f"  {i:3d}: {texture_name} {exists}")

    if len(env_data.texture_index_list) > max_count:
        print(f"  ... and {len(env_data.texture_index_list) - max_count} more")

    return env_data.texture_index_list


def convert_all_gtx_to_dds(game_path, progress_callback=None):
    """Convert all GTX files found in ENV to DDS format"""
    print("=== Converting All GTX Files to DDS ===")

    # Load ENV file to get texture list
    env_data = EnvFileData()
    env_path = get_env_file_path(game_path)
    if not env_path:
        print("ENV file not found. Check paths in addon preferences.")
        return []

    env_data.load(env_path)

    if not env_data.texture_index_list:
        print("No textures found in ENV file")
        return []

    # Get texture folder and DDS output paths
    texture_folder = get_texture_folder_path(game_path)
    if not texture_folder:
        print("Texture folder not found. Check paths in addon preferences.")
        return []

    dds_output_dir = get_dds_output_path()

    converted_textures = []
    total_textures = len(env_data.texture_index_list)

    for i, gtx_name in enumerate(env_data.texture_index_list):
        if progress_callback:
            progress_callback(i, total_textures, gtx_name)

        gtx_path = os.path.join(texture_folder, gtx_name)
        dds_path = os.path.join(dds_output_dir, gtx_name.replace('.gtx', '.dds'))

        # Skip if already converted
        if os.path.exists(dds_path):
            converted_textures.append({
                'index': i,
                'name': gtx_name,
                'dds_path': dds_path,
                'status': 'already_converted'
            })
            continue

        # Check if GTX file exists
        if not os.path.exists(gtx_path):
            converted_textures.append({
                'index': i,
                'name': gtx_name,
                'dds_path': None,
                'status': 'gtx_not_found'
            })
            continue

        # Convert GTX to DDS
        if convert_gtx_to_dds(gtx_path, dds_path, game_path):
            converted_textures.append({
                'index': i,
                'name': gtx_name,
                'dds_path': dds_path,
                'status': 'converted'
            })
            print(f"Converted {gtx_name} ({i+1}/{total_textures})")
        else:
            converted_textures.append({
                'index': i,
                'name': gtx_name,
                'dds_path': None,
                'status': 'conversion_failed'
            })
            print(f"Failed to convert {gtx_name}")

    # Summary
    converted_count = len([t for t in converted_textures if t['status'] in ['converted', 'already_converted']])
    print(f"Conversion complete: {converted_count}/{total_textures} textures available")

    return converted_textures


def get_env_texture_list(game_path):
    """Get the complete texture list from ENV file with conversion status"""
    env_data = EnvFileData()

    # Use path helpers to get correct paths
    env_path = get_env_file_path(game_path)
    if not env_path:
        print("ENV file not found. Check paths in addon preferences.")
        return []

    env_data.load(env_path)

    texture_folder = get_texture_folder_path(game_path)
    dds_output_dir = get_dds_output_path()
    texture_list = []

    for i, gtx_name in enumerate(env_data.texture_index_list):
        gtx_path = os.path.join(texture_folder, gtx_name) if texture_folder else ""
        dds_path = os.path.join(dds_output_dir, gtx_name.replace('.gtx', '.dds'))

        texture_info = {
            'index': i,
            'name': gtx_name.replace('.gtx', ''),
            'gtx_name': gtx_name,
            'gtx_path': gtx_path,
            'dds_path': dds_path,
            'gtx_exists': os.path.exists(gtx_path),
            'dds_exists': os.path.exists(dds_path),
            'can_convert': os.path.exists(gtx_path),
            'ready_for_use': os.path.exists(dds_path)
        }

        texture_list.append(texture_info)

    return texture_list


def get_kcm_map_info(filepath):
    """Get basic map information from a KCM file without full import"""
    try:
        kcm = KCMData()
        if kcm.load(filepath):
            return {
                'filepath': filepath,
                'filename': os.path.basename(filepath),
                'map_x': kcm.header['map_x'],
                'map_y': kcm.header['map_y'],
                'valid': True
            }
    except Exception as e:
        print(f"Failed to read KCM info from {filepath}: {e}")

    return {
        'filepath': filepath,
        'filename': os.path.basename(filepath),
        'map_x': 0,
        'map_y': 0,
        'valid': False
    }


def analyze_kcm_batch(filepaths):
    """Analyze a batch of KCM files and return map layout information"""
    print(f"=== Analyzing {len(filepaths)} KCM Files ===")

    map_info = []
    valid_maps = []

    for filepath in filepaths:
        info = get_kcm_map_info(filepath)
        map_info.append(info)
        if info['valid']:
            valid_maps.append(info)

    if not valid_maps:
        print("No valid KCM files found")
        return map_info, None

    # Calculate map bounds
    min_x = min(m['map_x'] for m in valid_maps)
    max_x = max(m['map_x'] for m in valid_maps)
    min_y = min(m['map_y'] for m in valid_maps)
    max_y = max(m['map_y'] for m in valid_maps)

    map_layout = {
        'total_files': len(filepaths),
        'valid_files': len(valid_maps),
        'map_bounds': {
            'min_x': min_x, 'max_x': max_x,
            'min_y': min_y, 'max_y': max_y,
            'width': max_x - min_x + 1,
            'height': max_y - min_y + 1
        },
        'coverage': {}
    }

    # Create coverage map
    for info in valid_maps:
        coord_key = f"{info['map_x']},{info['map_y']}"
        map_layout['coverage'][coord_key] = info['filename']

    print(f"Map Layout Analysis:")
    print(f"  Valid files: {len(valid_maps)}/{len(filepaths)}")
    print(f"  Map bounds: X({min_x} to {max_x}), Y({min_y} to {max_y})")
    print(f"  Map size: {map_layout['map_bounds']['width']} x {map_layout['map_bounds']['height']} tiles")

    # Show coverage grid
    print(f"  Coverage grid:")
    for y in range(max_y, min_y - 1, -1):  # Top to bottom
        row = f"    Y{y:2d}: "
        for x in range(min_x, max_x + 1):
            coord_key = f"{x},{y}"
            if coord_key in map_layout['coverage']:
                row += "■ "
            else:
                row += "□ "
        print(row)

    return map_info, map_layout


def create_map_overview_object(map_layout, terrain_grid_scale):
    """Create a simple overview object showing the map layout"""
    if not map_layout:
        return None

    # Adjust scale for 5% terrain size
    terrain_grid_scale = terrain_grid_scale * 0.05
    tile_size = 256 * terrain_grid_scale

    bounds = map_layout['map_bounds']

    # Create a simple plane for each map tile
    overview_name = f"Map_Overview_{bounds['width']}x{bounds['height']}"

    # Create collection for organization
    if overview_name not in bpy.data.collections:
        overview_collection = bpy.data.collections.new(overview_name)
        bpy.context.scene.collection.children.link(overview_collection)
    else:
        overview_collection = bpy.data.collections[overview_name]

    # Create overview plane
    bpy.ops.mesh.primitive_plane_add(
        size=1,
        location=(
            (bounds['min_x'] + bounds['max_x']) * tile_size / 2,
            (bounds['min_y'] + bounds['max_y']) * tile_size / 2,
            -1  # Slightly below terrain
        )
    )

    overview_obj = bpy.context.active_object
    overview_obj.name = overview_name
    overview_obj.scale = (
        bounds['width'] * tile_size / 2,
        bounds['height'] * tile_size / 2,
        1
    )

    # Move to overview collection
    bpy.context.scene.collection.objects.unlink(overview_obj)
    overview_collection.objects.link(overview_obj)

    # Add custom properties
    overview_obj['map_layout'] = {
        'total_files': map_layout['total_files'],
        'valid_files': map_layout['valid_files'],
        'bounds': bounds
    }

    print(f"Created map overview object: {overview_name}")
    return overview_obj

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