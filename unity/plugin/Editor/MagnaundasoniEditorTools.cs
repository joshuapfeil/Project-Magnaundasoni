// ============================================================================
// MagnaundasoniEditorTools.cs – Editor menu tools for material and geometry
// ============================================================================
using UnityEngine;
using UnityEditor;

namespace Magnaundasoni
{
    public static class MagnaundasoniEditorTools
    {
        // ----- Create Material Presets ------------------------------------
        [MenuItem("Magnaundasoni/Create Material Presets")]
        public static void CreateAllMaterialPresets()
        {
            string folder = EditorUtility.SaveFolderPanel(
                "Select folder for material presets", "Assets", "AcousticMaterials");
            if (string.IsNullOrEmpty(folder)) return;

            // Make relative to Assets
            if (folder.StartsWith(Application.dataPath))
                folder = "Assets" + folder.Substring(Application.dataPath.Length);

            string[] presets = MagnaundasoniMaterial.PresetNames;
            int created = 0;
            foreach (string preset in presets)
            {
                string path = $"{folder}/{preset}AcousticMaterial.asset";
                if (AssetDatabase.LoadAssetAtPath<MagnaundasoniMaterial>(path) != null)
                    continue;

                var mat = ScriptableObject.CreateInstance<MagnaundasoniMaterial>();
                mat.LoadPreset(preset);
                AssetDatabase.CreateAsset(mat, path);
                created++;
            }

            AssetDatabase.SaveAssets();
            AssetDatabase.Refresh();
            EditorUtility.DisplayDialog("Magnaundasoni",
                $"Created {created} material preset assets in {folder}.", "OK");
        }

        // ----- Assign Materials by Layer -----------------------------------
        [MenuItem("Magnaundasoni/Assign Materials by Layer")]
        public static void OpenLayerMaterialAssigner()
        {
            MagnaundasoniLayerAssignerWindow.ShowWindow();
        }

        // ----- Material Auditor --------------------------------------------
        [MenuItem("Magnaundasoni/Material Auditor")]
        public static void OpenMaterialAuditor()
        {
            MagnaundasoniMaterialAuditorWindow.ShowWindow();
        }

    }

    // -----------------------------------------------------------------------
    // Layer-to-Material Assignment Window
    // -----------------------------------------------------------------------
    public class MagnaundasoniLayerAssignerWindow : EditorWindow
    {
        private string[] _layers;
        private int[] _presetIndices;
        private static readonly string[] PresetOptions = new string[]
        {
            "(None)", "General", "Metal", "Wood", "Fabric",
            "Rock", "Dirt", "Grass", "Carpet", "Glass",
            "Concrete", "Plaster", "Water"
        };

        public static void ShowWindow()
        {
            GetWindow<MagnaundasoniLayerAssignerWindow>("Magnaundasoni Layer Material Assigner");
        }

        private void OnEnable()
        {
            _layers = new string[32];
            _presetIndices = new int[32];
            for (int i = 0; i < 32; i++)
            {
                _layers[i] = LayerMask.LayerToName(i);
                _presetIndices[i] = 0;
            }
        }

        private void OnGUI()
        {
            EditorGUILayout.LabelField("Map Unity Layers to Acoustic Material Presets",
                EditorStyles.boldLabel);
            EditorGUILayout.Space(8);

            EditorGUILayout.HelpBox(
                "Assign a default acoustic material preset to each Unity layer. " +
                "Objects on that layer will use this material unless they have an " +
                "explicit MagnaundasoniGeometry override.", MessageType.Info);

            EditorGUILayout.Space(4);

            for (int i = 0; i < 32; i++)
            {
                if (string.IsNullOrEmpty(_layers[i])) continue;

                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField($"Layer {i}: {_layers[i]}", GUILayout.Width(200));
                _presetIndices[i] = EditorGUILayout.Popup(_presetIndices[i], PresetOptions);
                EditorGUILayout.EndHorizontal();
            }

            EditorGUILayout.Space(8);
            if (GUILayout.Button("Apply to Scene Objects"))
            {
                ApplyLayerMaterials();
            }
        }

        private void ApplyLayerMaterials()
        {
            int applied = 0;
#if UNITY_2022_2_OR_NEWER
            var allGeometry = UnityEngine.Object.FindObjectsByType<MagnaundasoniGeometry>(FindObjectsSortMode.None);
#else
            var allGeometry = UnityEngine.Object.FindObjectsOfType<MagnaundasoniGeometry>();
#endif
            foreach (var geo in allGeometry)
            {
                int layer = geo.gameObject.layer;
                if (_presetIndices[layer] > 0)
                {
                    string presetName = PresetOptions[_presetIndices[layer]];
                    // Find or create a matching material asset
                    string assetPath = $"Assets/AcousticMaterials/{presetName}AcousticMaterial.asset";
                    var mat = AssetDatabase.LoadAssetAtPath<MagnaundasoniMaterial>(assetPath);
                    if (mat != null)
                    {
                        Undo.RecordObject(geo, "Assign Acoustic Material");
                        geo.SetMaterial(mat);
                        applied++;
                    }
                }
            }
            EditorUtility.DisplayDialog("Magnaundasoni",
                $"Applied materials to {applied} geometry objects.", "OK");
        }
    }

    // -----------------------------------------------------------------------
    // Material Auditor Window
    // -----------------------------------------------------------------------
    public class MagnaundasoniMaterialAuditorWindow : EditorWindow
    {
        private Vector2 _scrollPos;

        public static void ShowWindow()
        {
            GetWindow<MagnaundasoniMaterialAuditorWindow>("Magnaundasoni Material Auditor");
        }

        private void OnGUI()
        {
            EditorGUILayout.LabelField("Acoustic Material Auditor", EditorStyles.boldLabel);
            EditorGUILayout.Space(4);

#if UNITY_2022_2_OR_NEWER
            var allMeshRenderers = UnityEngine.Object.FindObjectsByType<MeshRenderer>(FindObjectsSortMode.None);
#else
            var allMeshRenderers = UnityEngine.Object.FindObjectsOfType<MeshRenderer>();
#endif
            int total = allMeshRenderers.Length;
            int withMaterial = 0;
            int withoutMaterial = 0;

            _scrollPos = EditorGUILayout.BeginScrollView(_scrollPos);

            EditorGUILayout.LabelField($"Total mesh renderers in scene: {total}");
            EditorGUILayout.Space(4);

            EditorGUILayout.LabelField("Objects without acoustic material:",
                EditorStyles.boldLabel);

            foreach (var mr in allMeshRenderers)
            {
                var geo = mr.GetComponent<MagnaundasoniGeometry>();
                if (geo != null)
                {
                    withMaterial++;
                }
                else
                {
                    withoutMaterial++;
                    EditorGUILayout.BeginHorizontal();
                    EditorGUILayout.ObjectField(mr.gameObject, typeof(GameObject), true);
                    if (GUILayout.Button("Add Geometry", GUILayout.Width(120)))
                    {
                        Undo.AddComponent<MagnaundasoniGeometry>(mr.gameObject);
                    }
                    EditorGUILayout.EndHorizontal();
                }
            }

            EditorGUILayout.Space(8);
            EditorGUILayout.LabelField($"With acoustic material: {withMaterial}");
            EditorGUILayout.LabelField($"Without acoustic material: {withoutMaterial}");
            EditorGUILayout.HelpBox(
                "Objects without a MagnaundasoniGeometry component will use the " +
                "auto-scan default material ('General') if auto-registration is enabled.",
                MessageType.Info);

            EditorGUILayout.EndScrollView();
        }
    }
}
