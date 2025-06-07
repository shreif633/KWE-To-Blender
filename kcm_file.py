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
        self.grass_list = []
        self.texture_index_list = []
        self.texture_resolutions = []  # Store width, height for each texture

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

                        # Read texture resolution (width and height as 4-byte integers)
                        width_bytes = f.read(4)
                        height_bytes = f.read(4)
                        if len(width_bytes) < 4 or len(height_bytes) < 4:
                            # If we can't read resolution, use default values
                            self.texture_resolutions.append((256, 256))
                            break

                        width = struct.unpack('<I', width_bytes)[0]
                        height = struct.unpack('<I', height_bytes)[0]
                        self.texture_resolutions.append((width, height))

                        print(f"  Texture {texture_count}: {text}.gtx ({width}x{height})")

                    except (struct.error, UnicodeDecodeError) as e:
                        print(f"Error reading texture {texture_count}: {e}")
                        break

                print(f"Successfully loaded ENV file:")
                print(f"  - {len(self.grass_list)} grass types")
                print(f"  - {len(self.texture_index_list)} textures")

                # Show first few textures with their resolutions for verification
                if self.texture_index_list:
                    print("First 10 textures with resolutions:")
                    for i, tex in enumerate(self.texture_index_list[:10]):
                        if i < len(self.texture_resolutions):
                            width, height = self.texture_resolutions[i]
                            print(f"  {i}: {tex} ({width}x{height})")
                        else:
                            print(f"  {i}: {tex} (no resolution data)")

                # Show resolution variety
                if self.texture_resolutions:
                    unique_resolutions = list(set(self.texture_resolutions))
                    print(f"Found {len(unique_resolutions)} different texture resolutions:")
                    for res in sorted(unique_resolutions):
                        count = self.texture_resolutions.count(res)
                        print(f"  {res[0]}x{res[1]}: {count} textures")

        except Exception as e:
            print(f"Failed to load ENV file {filepath}: {e}")
            import traceback
            traceback.print_exc()

# --- KCM Import Logic ---
def import_kcm(filepath, game_path, terrain_grid_scale, height_scale):
    kcm = KCMData()
    if not kcm.load(filepath): return

    # Use original Delphi 7 scaling from the source code
    # Original scaling: 1 Blender unit = 1 game meter
    # Height scale: Height values divided by 32 (line 824 in Unit1.pas: -KCM.HeightMap[x][y]/32)
    # Grid scale: Direct 1:1 mapping like original Delphi 7

    # Apply original Delphi 7 scaling first, then user scaling
    original_grid_scale = 1.0  # Original scale 1 from Delphi 7
    original_height_scale = 1.0 / 32.0  # Original height scale from Delphi 7

    # Apply user scaling on top of original Delphi 7 scaling
    final_grid_scale = original_grid_scale * terrain_grid_scale
    final_height_scale = original_height_scale * height_scale

    # Get map coordinates
    map_x = kcm.header['map_x']
    map_y = kcm.header['map_y']

    mesh_name = f"KCM_{map_x}_{map_y}"
    mesh = bpy.data.meshes.new(name=mesh_name)
    obj = bpy.data.objects.new(mesh_name, mesh)
    obj['kcm_header'] = kcm.header
    obj['kcm_import_grid_scale'] = final_grid_scale
    obj['kcm_import_height_scale'] = final_height_scale

    # Calculate terrain tile positioning based on original Delphi 7 code
    # Each terrain tile is 256x256 game units
    # From lines 3085-3087 in Unit1.pas:
    # X: position[0]*256 (direct)
    # Y: 256-(position[1]*256) (inverted!)
    # Z: position[2]/32 (height)
    tile_size = 256 * final_grid_scale

    # Position the terrain based on its X,Y coordinates like in original Delphi 7
    # From the original code: model.position.z = 256-(position[1]*256)
    # This means Y coordinates are inverted in the final positioning
    terrain_offset_x = map_x * tile_size
    terrain_offset_y = map_y * tile_size  # Keep positive, inversion happens in vertex calculation

    print(f"Importing KCM {map_x},{map_y} with original Delphi 7 coordinate system:")
    print(f"  User grid scale: {terrain_grid_scale:.3f} -> Final: {final_grid_scale:.6f}")
    print(f"  User height scale: {height_scale:.3f} -> Final: {final_height_scale:.6f}")
    print(f"  Original Delphi 7: Grid=1.0, Height=1/32={original_height_scale:.6f}")
    print(f"  Terrain offset: ({terrain_offset_x:.2f}, {terrain_offset_y:.2f})")
    print(f"  Coordinate mapping: Delphi(X,Y,Height) → Blender(X,Y-inverted,Z)")

    # Create vertices with proper world positioning using original coordinate system
    verts = []
    for y in range(257):
        for x in range(257):
            # Original Delphi 7 coordinate system from lines 3085-3087:
            # model.position.x = OPL.node[x1].position[0]*256     → Blender X
            # model.position.y = OPL.node[x1].position[2]/32      → Blender Z (height)
            # model.position.z = 256-(OPL.node[x1].position[1]*256) → Blender Y (inverted)

            # Correct coordinate mapping for terrain:
            # Delphi X → Blender X (direct)
            # Delphi Y → Blender Y (inverted: 256-y like Delphi Z calculation)
            # Delphi Height → Blender Z (height goes to Z axis)
            world_x = (x * final_grid_scale) + terrain_offset_x
            world_y = ((256 - y) * final_grid_scale) + terrain_offset_y  # Y inverted like original
            world_z = kcm.heightmap[x, y] * final_height_scale  # Height to Z axis
            verts.append(Vector((world_x, world_y, world_z)))

    # Create faces with proper winding order for Y-inverted coordinates
    faces = []
    for y in range(256):
        for x in range(256):
            # Vertex indices for quad (accounting for Y inversion)
            v1 = y * 257 + x
            v2 = y * 257 + (x + 1)
            v3 = (y + 1) * 257 + x
            v4 = (y + 1) * 257 + (x + 1)
            # Create quad with correct winding order
            faces.append((v1, v2, v4, v3))
    mesh.from_pydata(verts, [], faces)
    
    bm = bmesh.new(); bm.from_mesh(mesh)
    uv_layer = bm.loops.layers.uv.verify()
    for face in bm.faces:
        for loop in face.loops:
            # Account for Y coordinate inversion in UV mapping
            # Since Y coordinates are inverted (256-y), we need to flip V coordinate
            u = loop.vert.co.x / (256 * final_grid_scale)
            v = 1.0 - (loop.vert.co.y / (256 * final_grid_scale))  # Flip V coordinate
            loop[uv_layer].uv = (u, v)
    bm.to_mesh(mesh); bm.free()

    apply_greyscale_map(mesh, kcm.colormap, "Colormap_RGB")
    for i, tex_map in enumerate(kcm.texturemaps):
        if tex_map is not None: apply_greyscale_map(mesh, tex_map, f"BlendMap_{i}")
    
    try:
        create_terrain_material(obj, kcm, game_path)
    except Exception as e:
        print(f"Failed to create terrain material: {e}")
        import traceback
        traceback.print_exc()

    # Create collection for this KCM file based on filename
    kcm_filename = os.path.splitext(os.path.basename(filepath))[0]  # Get filename without extension
    collection_name = kcm_filename

    # Create collection if it doesn't exist
    if collection_name not in bpy.data.collections:
        kcm_collection = bpy.data.collections.new(collection_name)
        bpy.context.scene.collection.children.link(kcm_collection)
        print(f"Created collection: {collection_name}")
    else:
        kcm_collection = bpy.data.collections[collection_name]

    # Add object to the KCM-specific collection instead of scene collection
    kcm_collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    # Apply mesh smoothing and auto smooth shading for better terrain appearance
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

    # Set object to smooth shading (works in all Blender versions)
    bpy.ops.object.shade_smooth()

    # Apply auto smooth using modern Blender API (Blender 4.1+) or legacy method
    try:
        # Modern Blender 4.1+ method using modifier
        auto_smooth_modifier = obj.modifiers.new(name="AutoSmooth", type='SMOOTH')
        auto_smooth_modifier.angle = 1.0472  # 60 degrees in radians
        auto_smooth_modifier.use_smooth_shade = True
        print("Applied auto smooth using modern modifier")
    except:
        # Legacy method for older Blender versions
        try:
            mesh.use_auto_smooth = True
            mesh.auto_smooth_angle = 1.0472  # 60 degrees in radians
            print("Applied auto smooth using legacy method")
        except AttributeError:
            print("Auto smooth not available, using shade smooth only")

    # Apply actual mesh smoothing to geometry (smooth faces, edges, vertices)
    try:
        # Enter edit mode and smooth the mesh geometry
        bpy.ops.object.mode_set(mode='EDIT')
        bpy.ops.mesh.select_all(action='SELECT')

        # Apply Laplacian smooth to actually smooth the mesh geometry
        bpy.ops.mesh.vertices_smooth_laplacian(iterations=2, lambda_factor=0.5, lambda_border=0.5)

        # Also apply regular smooth for additional smoothing
        bpy.ops.mesh.vertices_smooth(factor=0.5, repeat=1)

        # Return to object mode
        bpy.ops.object.mode_set(mode='OBJECT')
        print("Applied mesh geometry smoothing")
    except Exception as e:
        print(f"Mesh smoothing failed: {e}")
        # Ensure we're back in object mode
        try:
            bpy.ops.object.mode_set(mode='OBJECT')
        except:
            pass

    mesh.update(); mesh.validate()

    print(f"Added {mesh_name} to collection '{collection_name}' with mesh smoothing and auto smooth shading")

def apply_greyscale_map(mesh, map_data, layer_name):
    if mesh.vertex_colors.get(layer_name): vcol_layer = mesh.vertex_colors[layer_name]
    else: vcol_layer = mesh.vertex_colors.new(name=layer_name)
    is_greyscale = len(map_data.shape) == 2
    for poly in mesh.polygons:
        for loop_index in poly.loop_indices:
            uv = mesh.uv_layers.active.data[loop_index].uv
            # Account for coordinate system inversion
            x = int(uv.x * 255)
            y = int((1.0 - uv.y) * 255)  # Flip Y coordinate back for texture sampling
            x, y = max(0, min(255, x)), max(0, min(255, y))
            if is_greyscale:
                val = map_data[x, y] / 255.0
                vcol_layer.data[loop_index].color = (val, val, val, 1.0)
            else:
                b, g, r = map_data[x, y]
                vcol_layer.data[loop_index].color = (r / 255.0, g / 255.0, b / 255.0, 1.0)

def create_terrain_material(obj, kcm, game_path):
    """Create multi-layer terrain material like original Kal World Editor"""
    env_data = EnvFileData()

    # Use path helpers to get correct ENV file path
    env_path = get_env_file_path(game_path)
    if not env_path:
        print(f"ENV file not found. Check paths in addon preferences.")
        return

    env_data.load(env_path)

    # Store texture info in object for later editing (only basic types allowed in ID properties)
    # Convert numpy integers to regular Python integers for Blender compatibility
    texture_list_converted = [int(x) for x in kcm.header['texture_list']]

    obj['kcm_textures'] = {
        'texture_list': texture_list_converted,
        'env_textures': env_data.texture_index_list,
        'texture_resolutions': env_data.texture_resolutions,
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
    bsdf.location, output.location = (1200, 0), (1400, 0)

    # Set material properties for natural terrain appearance
    bsdf.inputs['Metallic'].default_value = 1.0    # Full metallic for terrain materials
    bsdf.inputs['Roughness'].default_value = 1.0   # Full roughness for natural terrain look

    # Texture coordinate node
    tex_coord = nodes.new("ShaderNodeTexCoord")
    tex_coord.location = (-800, 400)

    # Mapping node for texture scaling
    mapping = nodes.new("ShaderNodeMapping")
    mapping.location = (-600, 400)
    mapping.inputs['Scale'].default_value = (1.0, 1.0, 1.0)
    links.new(tex_coord.outputs['UV'], mapping.inputs['Vector'])

    # Create multi-layer texture blending system (pass KCM data directly)
    create_multi_layer_texture_system(mat, kcm, env_data, game_path)

    # Connect to output
    links.new(bsdf.outputs['BSDF'], output.inputs['Surface'])

    # Setup texture paint slots for editing
    setup_texture_paint_slots(obj)

    # Set viewport shading to show textures properly
    setup_viewport_for_textures()

    # Verify material setup
    verify_material_setup(obj, mat)

    print(f"Created multi-layer terrain material for {obj.name}")


def create_multi_layer_texture_system(mat, kcm, env_data, game_path):
    """Create the multi-layer texture blending system like original Kal World Editor"""
    nodes = mat.node_tree.nodes
    links = mat.node_tree.links

    print(f"Creating multi-layer texture system...")
    print(f"Texture list: {kcm.header['texture_list']}")

    # Count available texture maps safely
    available_maps = 0
    for tm in kcm.texturemaps:
        if tm is not None:
            available_maps += 1
    print(f"Available texturemaps: {available_maps}")

    # Show resolution data availability
    print(f"ENV texture resolutions available: {len(env_data.texture_resolutions)}")
    if env_data.texture_resolutions:
        active_resolutions = []
        for i, tex_idx in enumerate(kcm.header['texture_list']):
            if tex_idx != 255 and tex_idx < len(env_data.texture_resolutions):
                active_resolutions.append(env_data.texture_resolutions[tex_idx])
        if active_resolutions:
            print(f"Active texture resolutions for this terrain: {active_resolutions}")

    # Get texture paths
    texture_folder = get_texture_folder_path(game_path)
    dds_output_dir = get_dds_output_path()

    if not texture_folder:
        print("Texture folder not found. Check paths in addon preferences.")
        return

    # Find mapping node
    mapping = nodes.get("Mapping")
    if not mapping:
        print("Mapping node not found")
        return

    # Create texture layers
    texture_layers = []
    blend_maps = []

    for i, tex_idx in enumerate(kcm.header['texture_list']):
        if tex_idx == 255:
            print(f"Skipping texture slot {i} (unused)")
            continue

        print(f"Processing texture slot {i}, index {tex_idx}")

        # Load texture with resolution information
        texture_node = create_texture_layer(nodes, links, mapping, i, tex_idx, env_data, texture_folder, dds_output_dir, game_path)
        if texture_node:
            texture_layers.append(texture_node)

            # Create blend map from KCM texture map data
            if i < len(kcm.texturemaps) and kcm.texturemaps[i] is not None:
                print(f"Creating blend map for texture {i}")
                blend_map = create_blend_map_image(kcm.texturemaps[i], f"BlendMap_{i}")
                if blend_map:
                    # Find the texture-specific mapping node for this layer
                    tex_mapping = nodes.get(f"Mapping_{i}")
                    if tex_mapping:
                        blend_node = create_blend_map_node(nodes, links, tex_mapping, blend_map, i)
                    else:
                        blend_node = create_blend_map_node(nodes, links, mapping, blend_map, i)
                    blend_maps.append(blend_node)
                    print(f"Created blend map node for texture {i}")
                else:
                    print(f"Failed to create blend map image for texture {i}")
                    blend_maps.append(None)
            else:
                print(f"No texture map data for texture {i}")
                blend_maps.append(None)
        else:
            print(f"Failed to create texture layer {i}")

    # Count blend maps safely
    blend_map_count = 0
    for bm in blend_maps:
        if bm is not None:
            blend_map_count += 1

    print(f"Created {len(texture_layers)} texture layers and {blend_map_count} blend maps")

    # Create blending chain
    if texture_layers:
        final_color = create_texture_blend_chain(nodes, links, texture_layers, blend_maps)

        # Connect to BSDF
        bsdf = nodes.get("Principled BSDF")
        if bsdf and final_color:
            links.new(final_color, bsdf.inputs['Base Color'])
            print("Connected final blended color to material")
        else:
            print("Failed to connect to BSDF - using first texture only")
            if texture_layers:
                links.new(texture_layers[0].outputs['Color'], bsdf.inputs['Base Color'])
    else:
        print("No texture layers created")


def create_texture_layer(nodes, links, mapping, layer_index, tex_idx, env_data, texture_folder, dds_output_dir, game_path):
    """Create a single texture layer with proper resolution-based scaling"""
    if tex_idx >= len(env_data.texture_index_list):
        return None

    gtx_name = env_data.texture_index_list[tex_idx]
    texture_name = gtx_name.replace('.gtx', '')

    # Get texture resolution from ENV data
    texture_width, texture_height = (256, 256)  # Default resolution
    if tex_idx < len(env_data.texture_resolutions):
        texture_width, texture_height = env_data.texture_resolutions[tex_idx]
        print(f"  Texture resolution from ENV: {texture_width}x{texture_height}")
    else:
        print(f"  Using default resolution: {texture_width}x{texture_height}")

    # Convert GTX to DDS if needed
    gtx_path = os.path.join(texture_folder, gtx_name)
    dds_path = os.path.join(dds_output_dir, gtx_name.replace('.gtx', '.dds'))

    if not os.path.exists(dds_path) and os.path.exists(gtx_path):
        if convert_gtx_to_dds(gtx_path, dds_path, game_path):
            print(f"Converted {gtx_name} to DDS")
        else:
            print(f"Failed to convert {gtx_name}")
            return None

    if not os.path.exists(dds_path):
        print(f"Texture not found: {gtx_name}")
        return None

    # Create texture coordinate mapping node for this specific texture
    tex_mapping = nodes.new('ShaderNodeMapping')
    tex_mapping.name = f"Mapping_{layer_index}"
    tex_mapping.label = f"Scale {layer_index}: {texture_name}"
    tex_mapping.location = (-600, -layer_index * 300)

    # Calculate texture scale based on resolution for natural game-like appearance
    # The game uses different texture scales based on resolution
    # Larger textures (like 512x512) should tile less than smaller ones (like 64x64)
    base_resolution = 256.0  # Base terrain resolution

    # Calculate individual scale factors for X and Y
    scale_x = (base_resolution / texture_width) * 8.0  # Multiply by 8 for better texture appearance
    scale_y = (base_resolution / texture_height) * 8.0  # Multiply by 8 for better texture appearance

    # Debug output to verify scaling calculation
    print(f"  Texture {texture_name}: {texture_width}x{texture_height}")
    print(f"  Scale calculation: ({base_resolution}/{texture_width}) * 8 = {scale_x:.3f}, ({base_resolution}/{texture_height}) * 8 = {scale_y:.3f}")

    # Apply enhanced texture scaling (8x multiplier)
    tex_mapping.inputs['Scale'].default_value = (scale_x, scale_y, 1.0)

    # Verify the scale was set correctly
    actual_scale = tex_mapping.inputs['Scale'].default_value
    print(f"  Applied scale: ({actual_scale[0]:.3f}, {actual_scale[1]:.3f}, {actual_scale[2]:.3f})")

    # Connect main UV mapping to this texture's mapping
    links.new(mapping.outputs['Vector'], tex_mapping.inputs['Vector'])

    # Create texture node
    tex_node = nodes.new('ShaderNodeTexImage')
    tex_node.name = f"Texture_{layer_index}"
    tex_node.label = f"Layer {layer_index}: {texture_name} ({texture_width}x{texture_height})"
    tex_node.location = (-200, -layer_index * 300)

    # Load image
    try:
        img = bpy.data.images.load(dds_path)
        img.name = f"KCM_Tex_{layer_index}_{texture_name}"
        img.colorspace_settings.name = 'sRGB'
        tex_node.image = img
        tex_node.interpolation = 'Linear'

        # Connect texture-specific UV mapping to texture node
        links.new(tex_mapping.outputs['Vector'], tex_node.inputs['Vector'])

        print(f"Created texture layer {layer_index}: {texture_name} ({texture_width}x{texture_height}) with scale ({scale_x:.2f}, {scale_y:.2f})")
        return tex_node

    except Exception as e:
        print(f"Failed to load texture {dds_path}: {e}")
        return None


def create_blend_map_image(texture_map_data, name):
    """Create a Blender image from KCM texture map data (256x256 grayscale)"""
    if texture_map_data is None:
        return None

    try:
        print(f"Creating blend map image: {name}")
        print(f"  Texture map data type: {type(texture_map_data)}")
        print(f"  Texture map data shape: {texture_map_data.shape if hasattr(texture_map_data, 'shape') else 'No shape'}")

        # Create new image
        img = bpy.data.images.new(name, width=256, height=256, alpha=False)

        # Convert texture map data to image pixels
        # KCM texture maps are 256x256 arrays with values 0-255
        pixels = []
        for y in range(256):
            for x in range(256):
                try:
                    # Get grayscale value (0-255) and convert to 0-1 range
                    value = float(texture_map_data[x][y]) / 255.0
                    # RGB + Alpha (grayscale, so R=G=B)
                    pixels.extend([value, value, value, 1.0])
                except Exception as pixel_error:
                    print(f"Error processing pixel ({x}, {y}): {pixel_error}")
                    # Use default gray value
                    pixels.extend([0.5, 0.5, 0.5, 1.0])

        # Set pixels
        img.pixels = pixels
        img.pack()  # Pack into blend file

        print(f"Successfully created blend map image: {name}")
        return img

    except Exception as e:
        print(f"Failed to create blend map {name}: {e}")
        import traceback
        traceback.print_exc()
        return None


def create_blend_map_node(nodes, links, mapping, blend_image, layer_index):
    """Create a blend map texture node"""
    blend_node = nodes.new('ShaderNodeTexImage')
    blend_node.name = f"BlendMap_{layer_index}"
    blend_node.label = f"Blend {layer_index}"
    blend_node.location = (-200, -layer_index * 300 - 150)
    blend_node.image = blend_image
    blend_node.interpolation = 'Linear'

    # Connect UV mapping (blend maps always use 1:1 mapping, not texture-specific scaling)
    # Find the main mapping node for blend maps
    main_mapping = nodes.get("Mapping")
    if main_mapping:
        links.new(main_mapping.outputs['Vector'], blend_node.inputs['Vector'])
    else:
        links.new(mapping.outputs['Vector'], blend_node.inputs['Vector'])

    return blend_node


def create_texture_blend_chain(nodes, links, texture_layers, blend_maps):
    """Create the texture blending chain like original Kal World Editor"""
    if not texture_layers:
        return None

    # Start with first texture as base
    current_color = texture_layers[0].outputs['Color']

    # Blend each subsequent layer
    for i in range(1, len(texture_layers)):
        if i < len(blend_maps) and blend_maps[i]:
            # Create mix node for blending
            mix_node = nodes.new('ShaderNodeMixRGB')
            mix_node.name = f"Mix_{i}"
            mix_node.label = f"Blend Layer {i}"
            mix_node.location = (0, -i * 200)
            mix_node.blend_type = 'MIX'

            # Connect textures and blend map
            links.new(current_color, mix_node.inputs['Color1'])  # Base
            links.new(texture_layers[i].outputs['Color'], mix_node.inputs['Color2'])  # New layer
            links.new(blend_maps[i].outputs['Color'], mix_node.inputs['Fac'])  # Blend factor

            current_color = mix_node.outputs['Color']
        else:
            # No blend map, just use the texture directly (shouldn't happen in normal KCM files)
            current_color = texture_layers[i].outputs['Color']

    return current_color


def verify_material_setup(obj, mat):
    """Verify that the material setup is correct and show scaling information"""
    print(f"=== Material Setup Verification for {obj.name} ===")

    if not mat or not mat.use_nodes:
        print("  ERROR: Material has no nodes")
        return

    nodes = mat.node_tree.nodes

    # Check for texture nodes and their scaling
    texture_nodes = [node for node in nodes if node.type == 'TEX_IMAGE' and node.name.startswith('Texture_')]
    mapping_nodes = [node for node in nodes if node.type == 'MAPPING' and node.name.startswith('Mapping_')]

    print(f"  Found {len(texture_nodes)} texture nodes")
    print(f"  Found {len(mapping_nodes)} mapping nodes")

    # Verify each texture has its own mapping with correct scale
    for tex_node in texture_nodes:
        layer_index = tex_node.name.split('_')[1]
        mapping_node = nodes.get(f"Mapping_{layer_index}")

        if mapping_node:
            scale = mapping_node.inputs['Scale'].default_value
            print(f"  Texture {layer_index}: Scale = ({scale[0]:.3f}, {scale[1]:.3f}, {scale[2]:.3f})")

            # Check if scale is different from 1.0 (indicating resolution-based scaling)
            if abs(scale[0] - 1.0) > 0.001 or abs(scale[1] - 1.0) > 0.001:
                print(f"    ✓ Custom scaling applied (resolution-based)")
            else:
                print(f"    ⚠ Default scaling (1.0) - may not be using resolution data")
        else:
            print(f"  Texture {layer_index}: ✗ No individual mapping node found")

    # Check blend maps
    blend_nodes = [node for node in nodes if node.type == 'TEX_IMAGE' and node.name.startswith('BlendMap_')]
    print(f"  Found {len(blend_nodes)} blend map nodes")

    # Check mix nodes
    mix_nodes = [node for node in nodes if node.type == 'MIX_RGB' and node.name.startswith('Mix_')]
    print(f"  Found {len(mix_nodes)} mix nodes for blending")

    print("=== End Verification ===")


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
    texture_resolutions = texture_data.get('texture_resolutions', [])

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
                'node_name': f"Texture_{i}",
                'env_resolution': (256, 256)  # Default resolution
            }

            if tex_idx < len(env_textures):
                tex_info['name'] = env_textures[tex_idx].replace('.gtx', '')

            # Get resolution from ENV data
            if tex_idx < len(texture_resolutions):
                tex_info['env_resolution'] = texture_resolutions[tex_idx]

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
            'gtx_exists': os.path.exists(gtx_path) if gtx_path else False,
            'dds_exists': os.path.exists(dds_path),
            'can_convert': os.path.exists(gtx_path) if gtx_path else False,
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

    # Create a simple plane for map overview
    overview_name = f"Map_Overview_{bounds['width']}x{bounds['height']}"

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

    # Create a simple material to make it visible
    mat = bpy.data.materials.new(name=f"{overview_name}_Material")
    mat.use_nodes = True
    mat.node_tree.nodes.clear()

    # Add emission shader for visibility
    emission = mat.node_tree.nodes.new(type='ShaderNodeEmission')
    emission.inputs['Color'].default_value = (0.2, 0.5, 1.0, 0.3)  # Light blue, semi-transparent
    emission.inputs['Strength'].default_value = 0.5

    output = mat.node_tree.nodes.new(type='ShaderNodeOutputMaterial')
    mat.node_tree.links.new(emission.outputs['Emission'], output.inputs['Surface'])

    # Enable transparency
    mat.blend_method = 'BLEND'

    # Assign material
    overview_obj.data.materials.append(mat)

    # Add custom properties
    overview_obj['map_layout'] = {
        'total_files': map_layout['total_files'],
        'valid_files': map_layout['valid_files'],
        'bounds': bounds
    }

    print(f"Created map overview object: {overview_name}")
    return overview_obj




def setup_texture_paint_slots(obj):
    """Setup texture paint slots for editing blend maps with brush tools"""
    if not obj or not obj.data.materials:
        return

    mat = obj.data.materials[0]
    if not mat or not mat.use_nodes:
        return

    print(f"Setting up texture paint slots for {obj.name}")

    # Clear existing paint slots
    while len(mat.texture_paint_slots) > 0:
        mat.texture_paint_slots.remove(mat.texture_paint_slots[0])

    # Create paint slots for each blend map image
    paint_slot_count = 0

    for i in range(7):
        # Find blend map node
        blend_node = mat.node_tree.nodes.get(f"BlendMap_{i}")
        if blend_node and blend_node.image:
            try:
                # Add texture paint slot
                slot = mat.texture_paint_slots.add()
                slot.name = f"Blend Layer {i}"

                # Make image editable and keep it in memory
                blend_node.image.use_fake_user = True

                paint_slot_count += 1
                print(f"Created paint slot for blend layer {i}")

            except Exception as e:
                print(f"Failed to create paint slot for layer {i}: {e}")

    # Setup UV map for painting
    if obj.data.uv_layers:
        obj.data.uv_layers.active = obj.data.uv_layers[0]

    print(f"Created {paint_slot_count} texture paint slots")


def enable_texture_paint_mode(obj):
    """Enable texture paint mode for editing terrain textures"""
    if not obj:
        return False

    try:
        # Set active object
        bpy.context.view_layer.objects.active = obj
        obj.select_set(True)

        # Switch to texture paint mode
        if bpy.context.mode != 'PAINT_TEXTURE':
            bpy.ops.object.mode_set(mode='TEXTURE_PAINT')

        # Configure brush settings for terrain painting
        if hasattr(bpy.context.tool_settings, 'image_paint'):
            brush = bpy.context.tool_settings.image_paint.brush
            if brush:
                brush.size = 50  # Medium brush size
                brush.strength = 0.5  # Medium strength
                brush.blend = 'MIX'  # Standard blending

        print(f"Enabled texture paint mode for {obj.name}")
        return True

    except Exception as e:
        print(f"Failed to enable texture paint mode: {e}")
        return False

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