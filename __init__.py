#
# Main Addon File: __init__.py
#
bl_info = {
    "name": "Kal Online Tools",
    "author": "KalOnline World Creator (By: Zelda)",
    "version": (2, 0, 0),
    "blender": (2, 80, 0),
    "location": "View3D > Sidebar > Kal Tools",
    "description": "Enhanced Kal Online tools with real texture display, ENV browser, and 5% terrain scaling.",
    "warning": "",
    "doc_url": "",
    "category": "Import-Export",
}

import bpy
from bpy.props import StringProperty, PointerProperty, FloatProperty, IntProperty, EnumProperty
from bpy_extras.io_utils import ImportHelper, ExportHelper
import os

# Import our custom modules
from . import kcm_file
from . import opl_file
from . import gtx_converter
from . import ksm_file

# Reload modules to ensure latest changes are loaded
import importlib
importlib.reload(kcm_file)

# --- Addon Preferences ---
class KalToolsPreferences(bpy.types.AddonPreferences):
    bl_idname = __name__

    game_path: StringProperty(
        name="Kal Online Client Path",
        description="Path to the root directory of your Kal Online client (e.g., C:/KalOnline)",
        subtype='DIR_PATH',
    )

    env_file_path: StringProperty(
        name="ENV File Path",
        description="Direct path to the n.env file - click folder icon to browse",
        subtype='FILE_PATH',
    )

    texture_folder_path: StringProperty(
        name="GTX Texture Folder",
        description="Path to the folder containing GTX texture files (e.g., C:/KalOnline/Data/3dData/Texture)",
        subtype='DIR_PATH',
    )

    dds_output_path: StringProperty(
        name="DDS Output Folder",
        description="Path where converted DDS files will be saved (leave empty to use temp folder)",
        subtype='DIR_PATH',
    )

    def draw(self, context):
        layout = self.layout

        # Main game path
        box = layout.box()
        box.label(text="Main Game Path:", icon='FOLDER_REDIRECT')
        box.prop(self, "game_path")
        box.label(text="Set this path to automatically find textures and encryption tables.")

        # Specific paths
        box = layout.box()
        box.label(text="Specific File Paths (Optional - Override automatic detection):", icon='FILE_FOLDER')

        row = box.row()
        col = row.column()
        col.prop(self, "env_file_path")
        col = row.column()
        col.operator("kal.browse_env_file", text="", icon='FILEBROWSER')

        if self.env_file_path:
            if os.path.exists(self.env_file_path):
                box.label(text=f"✓ ENV file found: {os.path.basename(self.env_file_path)}", icon='CHECKMARK')
            else:
                box.label(text=f"✗ ENV file not found: {self.env_file_path}", icon='ERROR')
        elif self.game_path:
            auto_env = os.path.join(self.game_path, "Data", "3dData", "Env", "g_env.env")
            box.label(text=f"Auto: {auto_env}", icon='INFO')

        row = box.row()
        col = row.column()
        col.prop(self, "texture_folder_path")
        col = row.column()
        col.operator("kal.browse_texture_folder", text="", icon='FILEBROWSER')

        if self.texture_folder_path:
            if os.path.exists(self.texture_folder_path):
                gtx_count = len([f for f in os.listdir(self.texture_folder_path) if f.lower().endswith('.gtx')])
                box.label(text=f"✓ Texture folder found: {gtx_count} GTX files", icon='CHECKMARK')
            else:
                box.label(text=f"✗ Texture folder not found: {self.texture_folder_path}", icon='ERROR')
        elif self.game_path:
            auto_tex = os.path.join(self.game_path, "Data", "3dData", "Texture")
            box.label(text=f"Auto: {auto_tex}", icon='INFO')

        row = box.row()
        row.prop(self, "dds_output_path")
        if not self.dds_output_path:
            auto_dds = os.path.join(bpy.app.tempdir, "kal_textures")
            row.label(text=f"Auto: {auto_dds}", icon='INFO')

        # Helper functions
        box = layout.box()
        box.label(text="Path Helpers:", icon='TOOL_SETTINGS')

        if self.game_path:
            col = box.column()
            col.operator("kal.auto_detect_paths", text="Auto-Detect All Paths", icon='FILE_REFRESH')
            col.operator("kal.validate_paths", text="Validate Paths", icon='CHECKMARK')


# --- Operators ---

class IMPORT_OT_kal_kcm(bpy.types.Operator, ImportHelper):
    """Import a Kal Online Client Map (.kcm)"""
    bl_idname = "import_scene.kal_kcm"
    bl_label = "Import KCM Map"
    filename_ext = ".kcm"
    filter_glob: StringProperty(default="*.kcm", options={'HIDDEN'})

    terrain_scale: FloatProperty(name="Terrain Scale", default=1.0, description="Scale of the terrain grid (1.0 = original Delphi 7 scale)")
    height_scale: FloatProperty(name="Height Scale", default=1.0, description="Multiplier for height values (1.0 = original Delphi 7 scale)")

    import_multiple: bpy.props.BoolProperty(
        name="Import Multiple KCM Files",
        description="Import multiple KCM files as map tiles positioned by their X,Y coordinates",
        default=False
    )

    files: bpy.props.CollectionProperty(
        name="File Path",
        type=bpy.types.OperatorFileListElement,
    )

    directory: StringProperty(
        subtype='DIR_PATH',
    )

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences

        if not prefs.game_path:
            self.report({'ERROR'}, "Game path not set. Please set it in the addon preferences.")
            return {'CANCELLED'}

        if self.import_multiple and self.files:
            # Import multiple KCM files as map tiles
            filepaths = []
            for file_elem in self.files:
                filepath = os.path.join(self.directory, file_elem.name)
                if filepath.lower().endswith('.kcm'):
                    filepaths.append(filepath)

            if not filepaths:
                self.report({'ERROR'}, "No KCM files selected")
                return {'CANCELLED'}

            # Analyze the batch first
            try:
                map_info, map_layout = kcm_file.analyze_kcm_batch(filepaths)

                if map_layout:
                    # Create map overview object
                    try:
                        kcm_file.create_map_overview_object(map_layout, self.terrain_scale)
                    except Exception as overview_error:
                        print(f"Warning: Could not create map overview: {overview_error}")
                        # Continue without overview
            except AttributeError as e:
                print(f"Function not found: {e}")
                print("Skipping batch analysis - importing files individually")
                map_layout = None
            except Exception as e:
                print(f"Batch analysis failed: {e}")
                print("Continuing with individual imports")
                map_layout = None

            # Import each file
            imported_count = 0
            failed_files = []
            created_collections = []

            for filepath in filepaths:
                try:
                    kcm_file.import_kcm(filepath, prefs.game_path, self.terrain_scale, self.height_scale)
                    imported_count += 1

                    # Track created collection
                    kcm_filename = os.path.splitext(os.path.basename(filepath))[0]
                    created_collections.append(kcm_filename)

                    print(f"Successfully imported: {os.path.basename(filepath)} → Collection: {kcm_filename}")
                except Exception as e:
                    failed_files.append(os.path.basename(filepath))
                    print(f"Failed to import {os.path.basename(filepath)}: {e}")

            # Report results
            if map_layout:
                bounds = map_layout['map_bounds']
                map_size_info = f" ({bounds['width']}x{bounds['height']} map)"
            else:
                map_size_info = ""

            if failed_files:
                self.report({'WARNING'}, f"Imported {imported_count} KCM tiles{map_size_info}. Failed: {', '.join(failed_files[:3])}{'...' if len(failed_files) > 3 else ''}")
            else:
                self.report({'INFO'}, f"Successfully imported {imported_count} KCM tiles{map_size_info}")

            # Show created collections and positioning info
            if created_collections:
                print(f"\nCreated {len(created_collections)} collections:")
                for collection_name in created_collections:
                    print(f"  - Collection: {collection_name}")

            kcm_objects = [obj for obj in bpy.data.objects if 'kcm_header' in obj]
            if kcm_objects:
                print(f"\nImported {len(kcm_objects)} KCM terrains:")
                for obj in kcm_objects[-imported_count:]:  # Show the newly imported ones
                    header = obj['kcm_header']
                    # Find which collection this object is in
                    obj_collection = "Unknown"
                    for collection in bpy.data.collections:
                        if obj.name in collection.objects:
                            obj_collection = collection.name
                            break
                    print(f"  - {obj.name}: Map({header['map_x']},{header['map_y']}) in Collection '{obj_collection}'")

            # Show water import summary for batch imports
            if imported_count > 1:  # Only show for multiple imports
                try:
                    kcm_file.print_water_import_summary()
                except Exception as e:
                    print(f"Could not generate water summary: {e}")
        else:
            # Import single KCM file
            try:
                kcm_file.import_kcm(self.filepath, prefs.game_path, self.terrain_scale, self.height_scale)
                self.report({'INFO'}, f"Imported KCM file: {os.path.basename(self.filepath)}")
            except Exception as e:
                self.report({'ERROR'}, f"Failed to import KCM file: {str(e)}")
                return {'CANCELLED'}

        return {'FINISHED'}

    def invoke(self, context, event):
        # Enable multiple file selection when import_multiple is True
        if hasattr(self, 'import_multiple') and self.import_multiple:
            context.window_manager.fileselect_add(self, accept_multiple=True)
        else:
            context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "terrain_scale")
        layout.prop(self, "height_scale")
        layout.separator()
        layout.prop(self, "import_multiple")
        if self.import_multiple:
            box = layout.box()
            box.label(text="Multiple KCM Import:", icon='INFO')
            box.label(text="• Select multiple .kcm files")
            box.label(text="• Files positioned by X,Y coordinates")
            box.label(text="• Creates connected map tiles")

class EXPORT_OT_kal_kcm(bpy.types.Operator, ExportHelper):
    """Export a Kal Online Client Map (.kcm)"""
    bl_idname = "export_scene.kal_kcm"
    bl_label = "Export KCM Map"
    filename_ext = ".kcm"
    filter_glob: StringProperty(default="*.kcm", options={'HIDDEN'})

    def execute(self, context):
        if not context.active_object or 'kcm_header' not in context.active_object:
            self.report({'ERROR'}, "Select an imported KCM terrain object to export.")
            return {'CANCELLED'}
            
        prefs = context.preferences.addons[__name__].preferences
        kcm_file.export_kcm(self.filepath, context.active_object, prefs.game_path)
        self.report({'INFO'}, f"Exported KCM to {self.filepath}")
        return {'FINISHED'}

class IMPORT_OT_kal_opl(bpy.types.Operator, ImportHelper):
    """Import a Kal Online Object Placement List (.opl)"""
    bl_idname = "import_scene.kal_opl"
    bl_label = "Import OPL Objects"
    filename_ext = ".opl"
    filter_glob: StringProperty(default="*.opl", options={'HIDDEN'})

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences
        opl_file.import_opl(self.filepath, prefs.game_path)
        return {'FINISHED'}

class EXPORT_OT_kal_opl(bpy.types.Operator, ExportHelper):
    """Export a Kal Online Object Placement List (.opl)"""
    bl_idname = "export_scene.kal_opl"
    bl_label = "Export OPL Objects"
    filename_ext = ".opl"
    filter_glob: StringProperty(default="*.opl", options={'HIDDEN'})

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences
        
        # Find the collection to export
        collection_name = f"OPL_{bpy.path.basename(self.filepath)}"
        collection = bpy.data.collections.get(collection_name)
        if not collection:
             self.report({'ERROR'}, f"Could not find collection '{collection_name}' to export.")
             return {'CANCELLED'}

        opl_file.export_opl(self.filepath, collection, prefs.game_path)
        self.report({'INFO'}, f"Exported OPL to {self.filepath}")
        return {'FINISHED'}


class IMPORT_OT_kal_ksm(bpy.types.Operator, ImportHelper):
    """Import a Kal Online Server Map (.ksm) as a Vertex Color layer"""
    bl_idname = "import_scene.kal_ksm"
    bl_label = "Import KSM Layer"
    filename_ext = ".ksm"
    filter_glob: StringProperty(default="*.ksm", options={'HIDDEN'})

    def execute(self, context):
        if not context.active_object or context.active_object.type != 'MESH':
            self.report({'ERROR'}, "Please select a terrain mesh object first.")
            return {'CANCELLED'}
        ksm_file.import_ksm_as_vcolor(self.filepath, context.active_object)
        return {'FINISHED'}


# --- Path Management Operators ---

class KAL_OT_auto_detect_paths(bpy.types.Operator):
    """Auto-detect ENV file and texture folder paths from game path"""
    bl_idname = "kal.auto_detect_paths"
    bl_label = "Auto-Detect Paths"
    bl_options = {'REGISTER'}

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences

        if not prefs.game_path:
            self.report({'ERROR'}, "Set main game path first.")
            return {'CANCELLED'}

        # Auto-detect ENV file path
        env_path = os.path.join(prefs.game_path, "Data", "3dData", "Env", "g_env.env")
        if os.path.exists(env_path):
            prefs.env_file_path = env_path
            self.report({'INFO'}, f"Found ENV file: {env_path}")
        else:
            self.report({'WARNING'}, f"ENV file not found at: {env_path}")

        # Auto-detect texture folder
        tex_folder = os.path.join(prefs.game_path, "Data", "3dData", "Texture")
        if os.path.exists(tex_folder):
            prefs.texture_folder_path = tex_folder
            self.report({'INFO'}, f"Found texture folder: {tex_folder}")
        else:
            self.report({'WARNING'}, f"Texture folder not found at: {tex_folder}")

        return {'FINISHED'}


class KAL_OT_validate_paths(bpy.types.Operator):
    """Validate all configured paths"""
    bl_idname = "kal.validate_paths"
    bl_label = "Validate Paths"
    bl_options = {'REGISTER'}

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences

        issues = []

        # Check main game path
        if not prefs.game_path:
            issues.append("Main game path not set")
        elif not os.path.exists(prefs.game_path):
            issues.append(f"Main game path does not exist: {prefs.game_path}")

        # Check ENV file path
        env_path = prefs.env_file_path or os.path.join(prefs.game_path, "Data", "3dData", "Env", "g_env.env")
        if not os.path.exists(env_path):
            issues.append(f"ENV file not found: {env_path}")

        # Check texture folder
        tex_folder = prefs.texture_folder_path or os.path.join(prefs.game_path, "Data", "3dData", "Texture")
        if not os.path.exists(tex_folder):
            issues.append(f"Texture folder not found: {tex_folder}")

        # Check DDS output folder
        dds_folder = prefs.dds_output_path or os.path.join(bpy.app.tempdir, "kal_textures")
        if prefs.dds_output_path and not os.path.exists(dds_folder):
            try:
                os.makedirs(dds_folder, exist_ok=True)
                self.report({'INFO'}, f"Created DDS output folder: {dds_folder}")
            except:
                issues.append(f"Cannot create DDS output folder: {dds_folder}")

        # Note: EnCrypt.dat and DeCrypt.dat are only needed for GTX conversion
        # ENV file reading doesn't require these files

        if issues:
            self.report({'ERROR'}, f"Path validation failed: {'; '.join(issues)}")
        else:
            self.report({'INFO'}, "All paths validated successfully!")

        return {'FINISHED'}


class KAL_OT_browse_env_file(bpy.types.Operator, ImportHelper):
    """Browse and select the n.env file"""
    bl_idname = "kal.browse_env_file"
    bl_label = "Select ENV File"
    bl_options = {'REGISTER'}

    filename_ext = ".env"
    filter_glob: StringProperty(default="*.env", options={'HIDDEN'})

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences
        prefs.env_file_path = self.filepath

        # Validate the selected file
        if os.path.exists(self.filepath):
            self.report({'INFO'}, f"ENV file set to: {self.filepath}")

            # Try to load and show basic info
            try:
                from . import kcm_file
                env_data = kcm_file.EnvFileData()
                env_data.load(self.filepath)
                texture_count = len(env_data.texture_index_list)
                grass_count = len(env_data.grass_list)
                self.report({'INFO'}, f"ENV file loaded: {texture_count} textures, {grass_count} grass types")
            except Exception as e:
                self.report({'WARNING'}, f"ENV file selected but validation failed: {str(e)}")
        else:
            self.report({'ERROR'}, f"Selected file does not exist: {self.filepath}")

        return {'FINISHED'}


class KAL_OT_browse_texture_folder(bpy.types.Operator):
    """Browse and select the GTX texture folder"""
    bl_idname = "kal.browse_texture_folder"
    bl_label = "Select Texture Folder"
    bl_options = {'REGISTER'}

    directory: StringProperty(subtype='DIR_PATH')

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences
        prefs.texture_folder_path = self.directory

        # Validate the selected folder
        if os.path.exists(self.directory):
            # Count GTX files in the folder
            gtx_files = [f for f in os.listdir(self.directory) if f.lower().endswith('.gtx')]
            self.report({'INFO'}, f"Texture folder set: {self.directory} ({len(gtx_files)} GTX files found)")
        else:
            self.report({'ERROR'}, f"Selected folder does not exist: {self.directory}")

        return {'FINISHED'}

    def invoke(self, context, event):
        context.window_manager.fileselect_add(self)
        return {'RUNNING_MODAL'}


# --- Enhanced Texture Management Operators ---

class KAL_OT_refresh_textures(bpy.types.Operator):
    """Refresh texture list for the selected KCM terrain"""
    bl_idname = "kal.refresh_textures"
    bl_label = "Refresh Textures"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object
        if not obj or 'kcm_textures' not in obj:
            self.report({'ERROR'}, "Select a KCM terrain object first.")
            return {'CANCELLED'}

        # Refresh the material
        if obj.data.materials:
            mat = obj.data.materials[0]
            kcm_data = obj.get('kcm_header', {})
            texture_data = obj.get('kcm_textures', {})
            game_path = texture_data.get('game_path', '')

            if game_path and kcm_data:
                # Recreate material with updated textures
                kcm_file.create_terrain_material(obj, type('KCM', (), {'header': kcm_data})(), game_path)
                self.report({'INFO'}, "Textures refreshed successfully.")
            else:
                self.report({'ERROR'}, "Missing texture or game path data.")

        return {'FINISHED'}


class KAL_OT_convert_all_gtx(bpy.types.Operator):
    """Convert all GTX files to DDS format"""
    bl_idname = "kal.convert_all_gtx"
    bl_label = "Convert All GTX to DDS"
    bl_options = {'REGISTER'}

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences
        game_path = prefs.game_path

        if not game_path:
            self.report({'ERROR'}, "Set game path in addon preferences first.")
            return {'CANCELLED'}

        def progress_callback(current, total, texture_name):
            progress = (current / total) * 100
            print(f"Converting {texture_name} ({current+1}/{total}) - {progress:.1f}%")

        try:
            converted_textures = kcm_file.convert_all_gtx_to_dds(game_path, progress_callback)

            converted_count = len([t for t in converted_textures if t['status'] in ['converted', 'already_converted']])
            self.report({'INFO'}, f"Converted {converted_count}/{len(converted_textures)} textures to DDS")

        except Exception as e:
            self.report({'ERROR'}, f"Conversion failed: {str(e)}")
            return {'CANCELLED'}

        return {'FINISHED'}


class KAL_OT_replace_texture_from_env(bpy.types.Operator):
    """Replace a terrain texture with one from the ENV texture list"""
    bl_idname = "kal.replace_texture_from_env"
    bl_label = "Replace with ENV Texture"
    bl_options = {'REGISTER', 'UNDO'}

    terrain_texture_index: IntProperty(
        name="Terrain Texture Slot",
        description="Which texture slot to replace (0-6)",
        default=0,
        min=0,
        max=6
    )

    env_texture_index: IntProperty(
        name="ENV Texture Index",
        description="Index of texture from ENV file to use",
        default=0,
        min=0
    )

    def execute(self, context):
        obj = context.active_object
        if not obj or 'kcm_textures' not in obj:
            self.report({'ERROR'}, "Select a KCM terrain object first.")
            return {'CANCELLED'}

        if not obj.data.materials:
            self.report({'ERROR'}, "No material found on terrain object.")
            return {'CANCELLED'}

        # Get game path and ENV texture list
        texture_data = obj.get('kcm_textures', {})
        game_path = texture_data.get('game_path', '')

        if not game_path:
            self.report({'ERROR'}, "No game path found in terrain data.")
            return {'CANCELLED'}

        try:
            env_textures = kcm_file.get_env_texture_list(game_path)

            if self.env_texture_index >= len(env_textures):
                self.report({'ERROR'}, f"ENV texture index {self.env_texture_index} out of range.")
                return {'CANCELLED'}

            env_texture = env_textures[self.env_texture_index]

            # Convert GTX to DDS if needed
            if not env_texture['ready_for_use'] and env_texture['can_convert']:
                from . import gtx_converter
                if gtx_converter.convert_gtx_to_dds(env_texture['gtx_path'], env_texture['dds_path'], game_path):
                    env_texture['ready_for_use'] = True
                else:
                    self.report({'ERROR'}, f"Failed to convert {env_texture['gtx_name']}")
                    return {'CANCELLED'}

            if not env_texture['ready_for_use']:
                self.report({'ERROR'}, f"Texture {env_texture['name']} is not available.")
                return {'CANCELLED'}

            # Replace the texture in the material
            mat = obj.data.materials[0]
            tex_node_name = f"Texture_{self.terrain_texture_index}"
            tex_node = mat.node_tree.nodes.get(tex_node_name)

            if not tex_node:
                self.report({'ERROR'}, f"Texture node {tex_node_name} not found.")
                return {'CANCELLED'}

            # Load the new texture
            new_img = bpy.data.images.load(env_texture['dds_path'])
            new_img.name = f"KCM_Tex_{self.terrain_texture_index}_{env_texture['name']}"
            new_img.colorspace_settings.name = 'sRGB'
            tex_node.image = new_img

            # Update the terrain's texture list
            texture_data['texture_list'][self.terrain_texture_index] = self.env_texture_index
            obj['kcm_textures'] = texture_data

            self.report({'INFO'}, f"Replaced texture {self.terrain_texture_index} with {env_texture['name']}")

        except Exception as e:
            self.report({'ERROR'}, f"Texture replacement failed: {str(e)}")
            return {'CANCELLED'}

        return {'FINISHED'}


# --- UI Panel ---
class KAL_PT_main_panel(bpy.types.Panel):
    bl_label = "Kal Online Tools"
    bl_idname = "KAL_PT_main_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Kal Tools'

    def draw(self, context):
        layout = self.layout
        
        box = layout.box()
        box.label(text="Map (.kcm)")
        row = box.row(align=True)
        row.operator(IMPORT_OT_kal_kcm.bl_idname, text="Import", icon='IMPORT')
        row.operator(EXPORT_OT_kal_kcm.bl_idname, text="Export", icon='EXPORT')

        box = layout.box()
        box.label(text="Objects (.opl)")
        row = box.row(align=True)
        row.operator(IMPORT_OT_kal_opl.bl_idname, text="Import", icon='IMPORT')
        row.operator(EXPORT_OT_kal_opl.bl_idname, text="Export", icon='EXPORT')
        
        box = layout.box()
        box.label(text="Server Data (.ksm)")
        box.operator(IMPORT_OT_kal_ksm.bl_idname, text="Import as VColor", icon='COLOR')

        box = layout.box()
        box.label(text="Utilities")
        box.operator("wm.url_open", text="Open Addon Prefs", icon='PREFERENCES').url = "bpy.ops.screen.userpref_show('INVOKE_DEFAULT', tab='ADDONS')"


class KAL_PT_texture_panel(bpy.types.Panel):
    bl_label = "KCM Texture Manager"
    bl_idname = "KAL_PT_texture_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Kal Tools'
    bl_parent_id = "KAL_PT_main_panel"

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == 'MESH' and 'kcm_textures' in obj

    def draw(self, context):
        layout = self.layout
        obj = context.active_object

        if not obj or 'kcm_textures' not in obj:
            layout.label(text="Select a KCM terrain object", icon='ERROR')
            return

        texture_data = obj.get('kcm_textures', {})
        texture_list = texture_data.get('texture_list', [])
        env_textures = texture_data.get('env_textures', [])

        # Header info
        box = layout.box()
        box.label(text=f"Terrain: {obj.name}", icon='MESH_PLANE')
        row = box.row(align=True)
        row.operator(KAL_OT_refresh_textures.bl_idname, text="Refresh", icon='FILE_REFRESH')
        row.operator(KAL_OT_enable_texture_paint.bl_idname, text="Paint Mode", icon='BRUSH_DATA')

        # Texture list
        box = layout.box()
        box.label(text="Textures Used:", icon='TEXTURE')

        if not obj.data.materials:
            box.label(text="No material found", icon='ERROR')
            return

        mat = obj.data.materials[0]

        for i, tex_idx in enumerate(texture_list):
            if tex_idx == 255:  # Skip unused texture slots
                continue

            row = box.row(align=True)

            # Texture info
            tex_name = "Unknown"
            if tex_idx < len(env_textures):
                tex_name = env_textures[tex_idx].replace('.gtx', '')

            # Check if texture node exists and has image
            tex_node = mat.node_tree.nodes.get(f"Texture_{i}")
            has_image = tex_node and tex_node.image

            # Get texture resolution from stored data
            texture_resolutions = texture_data.get('texture_resolutions', [])
            env_res = (256, 256)  # Default resolution
            if tex_idx < len(texture_resolutions):
                env_res = texture_resolutions[tex_idx]

            # Texture slot label with resolution info
            col = row.column()
            if has_image:
                col.label(text=f"{i}: {tex_name}", icon='TEXTURE_DATA')
                col.label(text=f"    Resolution: {env_res[0]}x{env_res[1]}", icon='BLANK1')
            else:
                col.label(text=f"{i}: {tex_name}", icon='ERROR')
                col.label(text=f"    Resolution: {env_res[0]}x{env_res[1]} (Missing)", icon='BLANK1')

            # Replace button
            col = row.column()
            replace_op = col.operator(KAL_OT_replace_texture_from_env.bl_idname, text="Replace", icon='FILEBROWSER')
            replace_op.terrain_texture_index = i
            replace_op.env_texture_index = tex_idx


class KAL_PT_env_texture_browser(bpy.types.Panel):
    bl_label = "ENV Texture Browser"
    bl_idname = "KAL_PT_env_texture_browser"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Kal Tools'
    bl_parent_id = "KAL_PT_main_panel"

    def draw(self, context):
        layout = self.layout
        prefs = context.preferences.addons[__name__].preferences

        if not prefs.game_path:
            box = layout.box()
            box.label(text="Set game path in addon preferences", icon='ERROR')
            return

        # Header with conversion tools
        box = layout.box()
        box.label(text="ENV Texture Management", icon='TEXTURE')
        box.operator(KAL_OT_convert_all_gtx.bl_idname, text="Convert All GTX to DDS", icon='IMPORT')

        # Get ENV texture list
        try:
            # Pass game_path to use with path helpers
            env_textures = kcm_file.get_env_texture_list(prefs.game_path)

            if not env_textures:
                box.label(text="No textures found in ENV file", icon='ERROR')
                return

            # Statistics
            ready_count = len([t for t in env_textures if t['ready_for_use']])
            convertible_count = len([t for t in env_textures if t['can_convert']])

            box = layout.box()
            box.label(text=f"Textures: {len(env_textures)} total", icon='INFO')
            box.label(text=f"Ready: {ready_count}, Convertible: {convertible_count}")

            # Show active terrain info if available
            obj = context.active_object
            if obj and 'kcm_textures' in obj:
                box.label(text=f"Active Terrain: {obj.name}", icon='MESH_PLANE')

            # Texture browser (limited to first 50 for performance)
            scroll_box = layout.box()
            scroll_box.label(text="Texture List (First 50):", icon='OUTLINER')

            for i, texture in enumerate(env_textures[:50]):
                row = scroll_box.row(align=True)

                # Status icon
                if texture['ready_for_use']:
                    icon = 'CHECKMARK'
                elif texture['can_convert']:
                    icon = 'IMPORT'
                else:
                    icon = 'ERROR'

                # Texture info
                col = row.column()
                col.scale_x = 0.3
                col.label(text=f"{i:3d}")

                col = row.column()
                # Get resolution info if available
                texture_data = obj.get('kcm_textures', {}) if obj else {}
                texture_resolutions = texture_data.get('texture_resolutions', [])
                resolution_text = ""
                if i < len(texture_resolutions):
                    width, height = texture_resolutions[i]
                    resolution_text = f" ({width}x{height})"

                col.label(text=f"{texture['name']}{resolution_text}", icon=icon)

                # Action buttons
                if obj and 'kcm_textures' in obj:
                    col = row.column()
                    col.scale_x = 0.3
                    use_op = col.operator(KAL_OT_replace_texture_from_env.bl_idname, text="Use", icon='PLUS')
                    use_op.env_texture_index = i
                    use_op.terrain_texture_index = 0  # Default to first slot

            if len(env_textures) > 50:
                scroll_box.label(text=f"... and {len(env_textures) - 50} more textures")
                scroll_box.label(text="Use Convert All GTX to prepare more textures")

        except Exception as e:
            box = layout.box()
            box.label(text=f"Error loading textures: {str(e)}", icon='ERROR')


class KAL_PT_map_manager(bpy.types.Panel):
    bl_label = "KCM Map Manager"
    bl_idname = "KAL_PT_map_manager"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'Kal Tools'
    bl_parent_id = "KAL_PT_main_panel"

    def draw(self, context):
        layout = self.layout

        # Count KCM objects and collections in scene
        kcm_objects = [obj for obj in bpy.data.objects if 'kcm_header' in obj]
        overview_objects = [obj for obj in bpy.data.objects if 'map_layout' in obj]

        # Count KCM collections (collections that contain KCM objects)
        kcm_collections = []
        for collection in bpy.data.collections:
            has_kcm = any('kcm_header' in obj for obj in collection.objects)
            if has_kcm:
                kcm_collections.append(collection.name)

        box = layout.box()
        box.label(text="Scene Map Status:", icon='WORLD')
        box.label(text=f"KCM Terrains: {len(kcm_objects)}")
        box.label(text=f"KCM Collections: {len(kcm_collections)}")
        box.label(text=f"Map Overviews: {len(overview_objects)}")

        if kcm_objects:
            # Show map coordinate range
            map_coords = [(obj['kcm_header']['map_x'], obj['kcm_header']['map_y']) for obj in kcm_objects]
            min_x = min(coord[0] for coord in map_coords)
            max_x = max(coord[0] for coord in map_coords)
            min_y = min(coord[1] for coord in map_coords)
            max_y = max(coord[1] for coord in map_coords)

            box.label(text=f"Map Range: X({min_x}-{max_x}), Y({min_y}-{max_y})")

            # Show first few terrains
            box = layout.box()
            box.label(text="Loaded Terrains:", icon='MESH_PLANE')

            for i, obj in enumerate(kcm_objects[:10]):  # Show first 10
                row = box.row()
                header = obj['kcm_header']
                row.label(text=f"{header['map_x']},{header['map_y']}: {obj.name}")

                # Select button
                select_op = row.operator("object.select_all", text="", icon='RESTRICT_SELECT_OFF')
                # Note: This is a simplified select - in a real implementation you'd create a custom operator

            if len(kcm_objects) > 10:
                box.label(text=f"... and {len(kcm_objects) - 10} more terrains")

        # Show KCM collections
        if kcm_collections:
            box = layout.box()
            box.label(text="KCM Collections:", icon='OUTLINER_COLLECTION')

            for i, collection_name in enumerate(kcm_collections[:10]):  # Show first 10
                row = box.row()
                row.label(text=f"{collection_name}")

                # Count objects in this collection
                collection = bpy.data.collections[collection_name]
                kcm_count = len([obj for obj in collection.objects if 'kcm_header' in obj])
                row.label(text=f"({kcm_count} terrain{'s' if kcm_count != 1 else ''})")

            if len(kcm_collections) > 10:
                box.label(text=f"... and {len(kcm_collections) - 10} more collections")

        # Map management tools
        box = layout.box()
        box.label(text="Map Tools:", icon='TOOL_SETTINGS')

        if kcm_objects:
            col = box.column()
            col.operator("kal.center_view_on_maps", text="Center View on Maps", icon='ZOOM_ALL')
            col.operator("kal.organize_map_collections", text="Organize in Collections", icon='OUTLINER')

        # Import tools
        box = layout.box()
        box.label(text="Import Multiple Maps:", icon='IMPORT')
        box.label(text="Use File > Import > Kal Online Map")
        box.label(text="Enable 'Import Multiple KCM Files'")
        box.label(text="Select multiple .kcm files for auto-positioning")


class KAL_OT_enable_texture_paint(bpy.types.Operator):
    """Enable texture paint mode for editing terrain blend maps"""
    bl_idname = "kal.enable_texture_paint"
    bl_label = "Enable Texture Paint"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.active_object

        if not obj or 'kcm_header' not in obj:
            self.report({'ERROR'}, "Please select a KCM terrain object")
            return {'CANCELLED'}

        try:
            # Setup texture paint slots
            kcm_file.setup_texture_paint_slots(obj)

            # Enable texture paint mode
            if kcm_file.enable_texture_paint_mode(obj):
                self.report({'INFO'}, f"Texture paint mode enabled for {obj.name}")

                # Show helpful info
                mat = obj.data.materials[0] if obj.data.materials else None
                if mat:
                    slot_count = len(mat.texture_paint_slots)
                    if slot_count > 0:
                        self.report({'INFO'}, f"Ready to paint {slot_count} blend layers")
                    else:
                        self.report({'WARNING'}, "No blend maps found to paint")
            else:
                self.report({'ERROR'}, "Failed to enable texture paint mode")
                return {'CANCELLED'}

        except Exception as e:
            self.report({'ERROR'}, f"Failed to setup texture painting: {str(e)}")
            return {'CANCELLED'}

        return {'FINISHED'}


# --- Registration ---
classes = (
    KalToolsPreferences,
    IMPORT_OT_kal_kcm,
    EXPORT_OT_kal_kcm,
    IMPORT_OT_kal_opl,
    EXPORT_OT_kal_opl,
    IMPORT_OT_kal_ksm,
    KAL_OT_auto_detect_paths,
    KAL_OT_validate_paths,
    KAL_OT_browse_env_file,
    KAL_OT_browse_texture_folder,
    KAL_OT_refresh_textures,
    KAL_OT_convert_all_gtx,
    KAL_OT_replace_texture_from_env,
    KAL_OT_enable_texture_paint,
    KAL_PT_main_panel,
    KAL_PT_texture_panel,
    KAL_PT_env_texture_browser,
    KAL_PT_map_manager,
)

def menu_func_import(self, context):
    self.layout.operator(IMPORT_OT_kal_kcm.bl_idname, text="Kal Online Map (.kcm)")
    self.layout.operator(IMPORT_OT_kal_opl.bl_idname, text="Kal Online Objects (.opl)")

def menu_func_export(self, context):
    self.layout.operator(EXPORT_OT_kal_kcm.bl_idname, text="Kal Online Map (.kcm)")
    self.layout.operator(EXPORT_OT_kal_opl.bl_idname, text="Kal Online Objects (.opl)")


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.TOPBAR_MT_file_import.append(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_func_export)

def unregister():
    bpy.types.TOPBAR_MT_file_import.remove(menu_func_import)
    bpy.types.TOPBAR_MT_file_export.remove(menu_func_export)
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

if __name__ == "__main__":
    register()