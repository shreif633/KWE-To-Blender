#
# Main Addon File: __init__.py
#
bl_info = {
    "name": "Kal Online Tools Enhanced",
    "author": "Enhanced by AI (Original Delphi conversion)",
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

# --- Addon Preferences ---
class KalToolsPreferences(bpy.types.AddonPreferences):
    bl_idname = __name__

    game_path: StringProperty(
        name="Kal Online Client Path",
        description="Path to the root directory of your Kal Online client (e.g., C:/KalOnline)",
        subtype='DIR_PATH',
    )

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "game_path")
        layout.label(text="Set this path to automatically find textures and encryption tables.")


# --- Operators ---

class IMPORT_OT_kal_kcm(bpy.types.Operator, ImportHelper):
    """Import a Kal Online Client Map (.kcm)"""
    bl_idname = "import_scene.kal_kcm"
    bl_label = "Import KCM Map"
    filename_ext = ".kcm"
    filter_glob: StringProperty(default="*.kcm", options={'HIDDEN'})
    
    terrain_scale: FloatProperty(name="Terrain Scale", default=10.0, description="Scale of the terrain grid")
    height_scale: FloatProperty(name="Height Scale", default=0.1, description="Multiplier for height values")

    def execute(self, context):
        prefs = context.preferences.addons[__name__].preferences
        kcm_file.import_kcm(self.filepath, prefs.game_path, self.terrain_scale, self.height_scale)
        return {'FINISHED'}

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
        box.operator(KAL_OT_refresh_textures.bl_idname, text="Refresh Textures", icon='FILE_REFRESH')

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

            # Texture slot label
            col = row.column()
            col.label(text=f"{i}: {tex_name}", icon='TEXTURE_DATA' if has_image else 'ERROR')

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
                col.label(text=texture['name'], icon=icon)

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


# --- Registration ---
classes = (
    KalToolsPreferences,
    IMPORT_OT_kal_kcm,
    EXPORT_OT_kal_kcm,
    IMPORT_OT_kal_opl,
    EXPORT_OT_kal_opl,
    IMPORT_OT_kal_ksm,
    KAL_OT_refresh_textures,
    KAL_OT_convert_all_gtx,
    KAL_OT_replace_texture_from_env,
    KAL_PT_main_panel,
    KAL_PT_texture_panel,
    KAL_PT_env_texture_browser,
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