#!/usr/bin/env python3
"""
Convert GLB (glTF binary) files to OBJ format for the renderer.
Requires: pip install trimesh
"""
import sys
import os

try:
    import trimesh
except ImportError:
    print("Error: trimesh not installed. Install with: pip install trimesh")
    sys.exit(1)

def convert_glb_to_obj(glb_path, obj_path):
    """Convert GLB file to OBJ file."""
    try:
        # Load GLB file
        scene = trimesh.load(glb_path)
        
        # If it's a scene with multiple meshes, combine them
        if isinstance(scene, trimesh.Scene):
            # Export as OBJ
            scene.export(obj_path, file_type='obj')
        elif isinstance(scene, trimesh.Trimesh):
            # Single mesh
            scene.export(obj_path, file_type='obj')
        else:
            # List of meshes
            combined = trimesh.util.concatenate(scene)
            combined.export(obj_path, file_type='obj')
        
        print(f"Successfully converted {glb_path} to {obj_path}")
        return True
    except Exception as e:
        print(f"Error converting {glb_path}: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 convert_glb_to_obj.py <input.glb> <output.obj>")
        sys.exit(1)
    
    glb_path = sys.argv[1]
    obj_path = sys.argv[2]
    
    if not os.path.exists(glb_path):
        print(f"Error: File not found: {glb_path}")
        sys.exit(1)
    
    convert_glb_to_obj(glb_path, obj_path)

