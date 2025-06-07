#
# Main Addon File: __init__.py
#
bl_info = {
    "name": "Kal Online Tools",
    "author": "Your Name (Converted from Delphi by AI)",
    "version": (1, 0, 0),
    "blender": (2, 80, 0),
    "location": "View3D > Sidebar > Kal Tools",
    "description": "Import, export, and edit Kal Online map and object files.",
    "warning": "",
    "doc_url": "",
    "category": "Import-Export",
}

import bpy
from bpy.props import StringProperty, PointerProperty, FloatProperty
from bpy_extras.io_utils import ImportHelper, ExportHelper

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


# --- Registration ---
classes = (
    KalToolsPreferences,
    IMPORT_OT_kal_kcm,
    EXPORT_OT_kal_kcm,
    IMPORT_OT_kal_opl,
    EXPORT_OT_kal_opl,
    IMPORT_OT_kal_ksm,
    KAL_PT_main_panel,
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