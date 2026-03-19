"""
setup_demo.py – Magnaundasoni Demo Map automation script.

Run this script from the Unreal Editor's built-in Python interpreter to
automatically create the four Blueprint classes and place them in the
currently-open level.

Usage (from the Editor's Python console / Output Log):
    import sys, importlib
    sys.path.append(r'<repo>/unreal/Content/MagnaundasoniDemo')
    import setup_demo; importlib.reload(setup_demo)
    setup_demo.run()

Requirements
------------
- Unreal Engine 5.2 or newer.
- Python Script Plugin enabled.
- Magnaundasoni plugin compiled and enabled.
- An empty level must be open (File → New Level → Empty Level).

The script uses the unreal.EditorAssetLibrary and unreal.EditorLevelLibrary
Python APIs that ship with the engine.  No third-party packages required.
"""

import unreal

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

CONTENT_PATH  = "/MagnaundasoniDemo"
LEVEL_PATH    = f"{CONTENT_PATH}/MagnaundasoniDemoLevel"

# UE class paths for the Magnaundasoni Runtime components.
MAG_SOURCE_CLASS    = "/Script/MagnaundasoniRuntime.MagSourceComponent"
MAG_LISTENER_CLASS  = "/Script/MagnaundasoniRuntime.MagListenerComponent"
MAG_GEOMETRY_CLASS  = "/Script/MagnaundasoniRuntime.MagGeometryComponent"

# ---------------------------------------------------------------------------
# Helper utilities
# ---------------------------------------------------------------------------

def _ensure_content_path() -> None:
    """Create the MagnaundasoniDemo content folder if it does not exist."""
    if not unreal.EditorAssetLibrary.does_directory_exist(CONTENT_PATH):
        unreal.EditorAssetLibrary.make_directory(CONTENT_PATH)


def _create_blueprint(name: str, parent_class_path: str) -> unreal.Blueprint:
    """
    Create a new Blueprint asset under CONTENT_PATH if it does not already
    exist.  Returns the Blueprint object.
    """
    asset_path = f"{CONTENT_PATH}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        unreal.log(f"[setup_demo] {name} already exists – skipping creation.")
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class",
                                unreal.load_class(None, parent_class_path))
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    bp = asset_tools.create_asset(name, CONTENT_PATH, None, factory)
    unreal.EditorAssetLibrary.save_asset(f"{CONTENT_PATH}/{name}")
    unreal.log(f"[setup_demo] Created Blueprint: {CONTENT_PATH}/{name}")
    return bp


def _add_component(bp_cdo: unreal.Object, comp_class_path: str,
                    comp_name: str) -> unreal.ActorComponent:
    """
    Add a component to a Blueprint's Class Default Object (CDO).
    Returns the created component, or None if it already exists.
    """
    comp_class = unreal.load_class(None, comp_class_path)
    if comp_class is None:
        unreal.log_warning(f"[setup_demo] Component class not found: {comp_class_path}")
        return None

    # Use the subsystem helper to add a component to a Blueprint CDO.
    subsystem = unreal.get_editor_subsystem(unreal.SubobjectDataSubsystem)
    root_handle = subsystem.k2_gather_subobject_data_for_blueprint(bp_cdo)[0]
    return subsystem.add_new_subobject(
        unreal.AddNewSubobjectParams(
            parent_handle=root_handle,
            new_class=comp_class,
            blueprint_context=bp_cdo
        )
    ).subobject_handle


# ---------------------------------------------------------------------------
# Blueprint creation functions
# ---------------------------------------------------------------------------

def _create_bp_mag_enemy() -> None:
    """
    BP_MagEnemy – Patrolling enemy pawn with acoustic source.
    Hierarchy: DefaultSceneRoot → StaticMesh (capsule) → AudioComponent → MagSourceComponent
    """
    bp = _create_blueprint("BP_MagEnemy", "/Script/Engine.Pawn")
    if bp is None:
        return

    cdo = unreal.get_default_object(bp.generated_class())
    bp_gc = bp.generated_class()

    # Add a StaticMeshComponent as the visual representation.
    mesh_handle = _add_component(bp_gc, "/Script/Engine.StaticMeshComponent", "EnemyMesh")
    # Assign the engine sphere mesh as a placeholder.
    sphere_mesh = unreal.load_asset("/Engine/BasicShapes/Sphere")
    if mesh_handle and sphere_mesh:
        unreal.SubobjectDataBlueprintFunctionLibrary.set_object_sub_object(
            mesh_handle, sphere_mesh)

    # Add an AudioComponent for the footstep/ambient sound.
    _add_component(bp_gc, "/Script/Engine.AudioComponent", "EnemyAudio")

    # Add the Magnaundasoni Source component.
    src_handle = _add_component(bp_gc, MAG_SOURCE_CLASS, "MagSource")
    if src_handle:
        src_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(src_handle)
        if src_obj:
            src_obj.set_editor_property("importance_class",
                unreal.MagImportanceClass.DYNAMIC_IMPORTANT)
            src_obj.set_editor_property("radius_cm", 30.0)
            src_obj.set_editor_property("b_auto_apply_to_audio_component", True)

    unreal.EditorAssetLibrary.save_asset(f"{CONTENT_PATH}/BP_MagEnemy")
    unreal.log("[setup_demo] BP_MagEnemy configured.")


def _create_bp_mag_door() -> None:
    """
    BP_MagDoor – Sliding door actor with dynamic acoustic geometry.
    Hierarchy: DefaultSceneRoot → DoorMesh (StaticMesh) → MagGeometryComponent
    """
    bp = _create_blueprint("BP_MagDoor", "/Script/Engine.Actor")
    if bp is None:
        return

    bp_gc = bp.generated_class()

    # Door mesh (thin box placeholder).
    mesh_handle = _add_component(bp_gc, "/Script/Engine.StaticMeshComponent", "DoorMesh")
    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube")
    if mesh_handle and cube_mesh:
        unreal.SubobjectDataBlueprintFunctionLibrary.set_object_sub_object(
            mesh_handle, cube_mesh)
        mesh_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(mesh_handle)
        if mesh_obj:
            # Scale to door-like proportions (10cm × 200cm × 100cm) in UE units.
            mesh_obj.set_editor_property("relative_scale3d",
                unreal.Vector(0.1, 2.0, 1.0))

    # Acoustic geometry component.
    geom_handle = _add_component(bp_gc, MAG_GEOMETRY_CLASS, "MagGeometry")
    if geom_handle:
        geom_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(geom_handle)
        if geom_obj:
            geom_obj.set_editor_property("material_preset", "Wood")
            geom_obj.set_editor_property("importance_class",
                unreal.MagImportanceClass.DYNAMIC_IMPORTANT)
            geom_obj.set_editor_property("b_auto_register", True)

    unreal.EditorAssetLibrary.save_asset(f"{CONTENT_PATH}/BP_MagDoor")
    unreal.log("[setup_demo] BP_MagDoor configured.")


def _create_bp_mag_player() -> None:
    """
    BP_MagPlayer – Player character with primary acoustic listener.
    Hierarchy: DefaultSceneRoot → CameraComponent → MagListenerComponent
    """
    bp = _create_blueprint("BP_MagPlayer", "/Script/Engine.Character")
    if bp is None:
        return

    bp_gc = bp.generated_class()

    # Listener component attached to the camera.
    listener_handle = _add_component(bp_gc, MAG_LISTENER_CLASS, "MagListener")
    if listener_handle:
        listener_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(
            listener_handle)
        if listener_obj:
            listener_obj.set_editor_property("b_is_primary_listener", True)

    unreal.EditorAssetLibrary.save_asset(f"{CONTENT_PATH}/BP_MagPlayer")
    unreal.log("[setup_demo] BP_MagPlayer configured.")


def _create_bp_mag_walls() -> None:
    """
    BP_MagWalls – Simple room geometry actor (placeholder cube set to room scale).
    Hierarchy: DefaultSceneRoot → RoomMesh (StaticMesh) → MagGeometryComponent
    """
    bp = _create_blueprint("BP_MagWalls", "/Script/Engine.Actor")
    if bp is None:
        return

    bp_gc = bp.generated_class()

    mesh_handle = _add_component(bp_gc, "/Script/Engine.StaticMeshComponent", "RoomMesh")
    cube_mesh = unreal.load_asset("/Engine/BasicShapes/Cube")
    if mesh_handle and cube_mesh:
        unreal.SubobjectDataBlueprintFunctionLibrary.set_object_sub_object(
            mesh_handle, cube_mesh)
        mesh_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(mesh_handle)
        if mesh_obj:
            # 20 m × 20 m × 5 m room.
            mesh_obj.set_editor_property("relative_scale3d",
                unreal.Vector(20.0, 20.0, 5.0))

    geom_handle = _add_component(bp_gc, MAG_GEOMETRY_CLASS, "MagGeometry")
    if geom_handle:
        geom_obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(geom_handle)
        if geom_obj:
            geom_obj.set_editor_property("material_preset", "Concrete")
            geom_obj.set_editor_property("importance_class",
                unreal.MagImportanceClass.STATIC)
            geom_obj.set_editor_property("b_auto_register", True)

    unreal.EditorAssetLibrary.save_asset(f"{CONTENT_PATH}/BP_MagWalls")
    unreal.log("[setup_demo] BP_MagWalls configured.")


# ---------------------------------------------------------------------------
# Level population
# ---------------------------------------------------------------------------

def _populate_level() -> None:
    """Spawn one instance of each Blueprint into the currently open level."""
    level_lib = unreal.EditorLevelLibrary

    actors_to_spawn = [
        ("BP_MagWalls",  unreal.Vector(0,    0,   0),   unreal.Rotator(0, 0, 0)),
        ("BP_MagDoor",   unreal.Vector(500,  0, 100),   unreal.Rotator(0, 0, 0)),
        ("BP_MagEnemy",  unreal.Vector(-400, 200, 50),  unreal.Rotator(0, 0, 0)),
        ("BP_MagPlayer", unreal.Vector(0,  -300, 100),  unreal.Rotator(0, 0, 0)),
    ]

    for (bp_name, location, rotation) in actors_to_spawn:
        bp_class = unreal.load_class(None, f"{CONTENT_PATH}/{bp_name}.{bp_name}_C")
        if bp_class is None:
            unreal.log_warning(
                f"[setup_demo] Could not load class {bp_name}; skipping placement.")
            continue
        spawned = level_lib.spawn_actor_from_class(bp_class, location, rotation)
        if spawned:
            spawned.set_actor_label(bp_name)
            unreal.log(f"[setup_demo] Spawned {bp_name} at {location}.")
        else:
            unreal.log_warning(f"[setup_demo] Failed to spawn {bp_name}.")


# ---------------------------------------------------------------------------
# Main entry point
# ---------------------------------------------------------------------------

def run() -> None:
    """Create all demo Blueprints and populate the current level."""
    unreal.log("[setup_demo] === Magnaundasoni Demo Setup ===")

    with unreal.ScopedSlowTask(5, "Setting up Magnaundasoni Demo") as task:
        task.make_dialog(True)

        task.enter_progress_frame(1, "Preparing content directory...")
        _ensure_content_path()

        task.enter_progress_frame(1, "Creating BP_MagWalls...")
        _create_bp_mag_walls()

        task.enter_progress_frame(1, "Creating BP_MagDoor...")
        _create_bp_mag_door()

        task.enter_progress_frame(1, "Creating BP_MagEnemy...")
        _create_bp_mag_enemy()

        task.enter_progress_frame(1, "Creating BP_MagPlayer and populating level...")
        _create_bp_mag_player()
        _populate_level()

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[setup_demo] === Done! Press Alt+P to play the demo. ===")
    unreal.log("[setup_demo] Tip: open the Output Log and type:")
    unreal.log("[setup_demo]   magn.Debug.ShowRays 1")
    unreal.log("[setup_demo]   magn.Debug.ShowStats 1")
