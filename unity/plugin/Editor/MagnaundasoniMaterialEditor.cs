// ============================================================================
// MagnaundasoniMaterialEditor.cs – Custom editor for MagnaundasoniMaterial
// ============================================================================
using UnityEngine;
using UnityEditor;

namespace Magnaundasoni
{
    [CustomEditor(typeof(MagnaundasoniMaterial))]
    public class MagnaundasoniMaterialEditor : Editor
    {
        // Frequency labels for the 8-band editor
        private static readonly string[] BandLabels = new string[]
        {
            "63 Hz", "125 Hz", "250 Hz", "500 Hz",
            "1 kHz", "2 kHz", "4 kHz", "8 kHz"
        };

        private static readonly Color AbsorptionColor = new Color(0.2f, 0.6f, 1f, 1f);
        private static readonly Color TransmissionColor = new Color(1f, 0.4f, 0.3f, 1f);
        private static readonly Color ScatteringColor = new Color(0.3f, 0.9f, 0.4f, 1f);

        private SerializedProperty _absorption;
        private SerializedProperty _transmission;
        private SerializedProperty _scattering;
        private SerializedProperty _roughness;
        private SerializedProperty _thicknessClass;
        private SerializedProperty _leakage;
        private SerializedProperty _categoryTag;

        private int _selectedPresetIndex = -1;
        private bool _showAbsorption = true;
        private bool _showTransmission = true;
        private bool _showScattering = true;

        private void OnEnable()
        {
            _absorption = serializedObject.FindProperty("_absorption");
            _transmission = serializedObject.FindProperty("_transmission");
            _scattering = serializedObject.FindProperty("_scattering");
            _roughness = serializedObject.FindProperty("_roughness");
            _thicknessClass = serializedObject.FindProperty("_thicknessClass");
            _leakage = serializedObject.FindProperty("_leakage");
            _categoryTag = serializedObject.FindProperty("_categoryTag");
        }

        public override void OnInspectorGUI()
        {
            serializedObject.Update();

            DrawPresetDropdown();
            EditorGUILayout.Space(8);

            DrawBandSection("Absorption", _absorption, ref _showAbsorption, AbsorptionColor);
            DrawBandSection("Transmission", _transmission, ref _showTransmission, TransmissionColor);
            DrawBandSection("Scattering", _scattering, ref _showScattering, ScatteringColor);

            EditorGUILayout.Space(8);
            DrawCurveVisualization();

            EditorGUILayout.Space(8);
            EditorGUILayout.LabelField("Surface Properties", EditorStyles.boldLabel);
            EditorGUILayout.PropertyField(_roughness);
            EditorGUILayout.PropertyField(_thicknessClass, new GUIContent("Thickness Class (0=Thin, 1=Std, 2=Thick)"));
            EditorGUILayout.PropertyField(_leakage, new GUIContent("Leakage Hint"));
            EditorGUILayout.PropertyField(_categoryTag);

            DrawRegistrationStatus();

            serializedObject.ApplyModifiedProperties();
        }

        // ----- Preset Dropdown ---------------------------------------------
        private void DrawPresetDropdown()
        {
            EditorGUILayout.LabelField("Material Preset", EditorStyles.boldLabel);

            string[] presetNames = MagnaundasoniMaterial.PresetNames;
            string[] displayNames = new string[presetNames.Length + 1];
            displayNames[0] = "(Select Preset)";
            for (int i = 0; i < presetNames.Length; i++)
                displayNames[i + 1] = ObjectNames.NicifyVariableName(presetNames[i]);

            int newIndex = EditorGUILayout.Popup("Load Preset", _selectedPresetIndex + 1, displayNames);
            if (newIndex > 0 && newIndex != _selectedPresetIndex + 1)
            {
                _selectedPresetIndex = newIndex - 1;
                var mat = (MagnaundasoniMaterial)target;
                Undo.RecordObject(mat, "Load Acoustic Preset");
                mat.LoadPreset(presetNames[_selectedPresetIndex]);
                serializedObject.Update();
            }
        }

        // ----- Band Sliders ------------------------------------------------
        private void DrawBandSection(string label, SerializedProperty arrayProp,
            ref bool foldout, Color accentColor)
        {
            Color prevBg = GUI.backgroundColor;
            GUI.backgroundColor = accentColor;
            foldout = EditorGUILayout.BeginFoldoutHeaderGroup(foldout, label);
            GUI.backgroundColor = prevBg;

            if (foldout && arrayProp != null)
            {
                EditorGUI.indentLevel++;
                int count = Mathf.Min(arrayProp.arraySize, MagConstants.MaxBands);
                for (int i = 0; i < count; i++)
                {
                    var elem = arrayProp.GetArrayElementAtIndex(i);
                    EditorGUILayout.BeginHorizontal();
                    EditorGUILayout.LabelField(BandLabels[i], GUILayout.Width(60));
                    elem.floatValue = EditorGUILayout.Slider(elem.floatValue, 0f, 1f);
                    EditorGUILayout.EndHorizontal();
                }
                EditorGUI.indentLevel--;
            }

            EditorGUILayout.EndFoldoutHeaderGroup();
        }

        // ----- Curve Visualization -----------------------------------------
        private void DrawCurveVisualization()
        {
            EditorGUILayout.LabelField("Frequency Response Curves", EditorStyles.boldLabel);

            Rect rect = GUILayoutUtility.GetRect(GUIContent.none, GUIStyle.none,
                GUILayout.Height(120), GUILayout.ExpandWidth(true));

            if (Event.current.type != EventType.Repaint) return;

            // Background
            EditorGUI.DrawRect(rect, new Color(0.15f, 0.15f, 0.15f, 1f));

            // Grid lines
            Handles.color = new Color(1f, 1f, 1f, 0.1f);
            for (int i = 0; i <= 4; i++)
            {
                float y = rect.y + rect.height * (1f - i / 4f);
                Handles.DrawLine(new Vector3(rect.x, y), new Vector3(rect.xMax, y));
            }
            for (int i = 0; i < MagConstants.MaxBands; i++)
            {
                float x = rect.x + rect.width * i / (MagConstants.MaxBands - 1f);
                Handles.DrawLine(new Vector3(x, rect.y), new Vector3(x, rect.yMax));
            }

            // Draw curves
            DrawCurve(rect, _absorption, AbsorptionColor);
            DrawCurve(rect, _transmission, TransmissionColor);
            DrawCurve(rect, _scattering, ScatteringColor);

            // Legend
            float legendX = rect.x + 4;
            float legendY = rect.y + 4;
            DrawLegendItem(legendX, legendY, "Absorption", AbsorptionColor);
            DrawLegendItem(legendX, legendY + 14, "Transmission", TransmissionColor);
            DrawLegendItem(legendX, legendY + 28, "Scattering", ScatteringColor);
        }

        private void DrawCurve(Rect rect, SerializedProperty arrayProp, Color color)
        {
            if (arrayProp == null) return;

            int count = Mathf.Min(arrayProp.arraySize, MagConstants.MaxBands);
            if (count < 2) return;

            Handles.color = color;
            for (int i = 0; i < count - 1; i++)
            {
                float x0 = rect.x + rect.width * i / (count - 1f);
                float y0 = rect.yMax - rect.height * Mathf.Clamp01(arrayProp.GetArrayElementAtIndex(i).floatValue);
                float x1 = rect.x + rect.width * (i + 1) / (count - 1f);
                float y1 = rect.yMax - rect.height * Mathf.Clamp01(arrayProp.GetArrayElementAtIndex(i + 1).floatValue);
                Handles.DrawLine(new Vector3(x0, y0), new Vector3(x1, y1));
            }

            // Draw points
            for (int i = 0; i < count; i++)
            {
                float x = rect.x + rect.width * i / (count - 1f);
                float y = rect.yMax - rect.height * Mathf.Clamp01(arrayProp.GetArrayElementAtIndex(i).floatValue);
                Rect dotRect = new Rect(x - 3, y - 3, 6, 6);
                EditorGUI.DrawRect(dotRect, color);
            }
        }

        private void DrawLegendItem(float x, float y, string label, Color color)
        {
            EditorGUI.DrawRect(new Rect(x, y, 10, 10), color);
            GUI.Label(new Rect(x + 14, y - 2, 100, 16), label, EditorStyles.miniLabel);
        }

        // ----- Registration Status -----------------------------------------
        private void DrawRegistrationStatus()
        {
            var mat = (MagnaundasoniMaterial)target;
            EditorGUILayout.Space(4);
            EditorGUILayout.BeginHorizontal(EditorStyles.helpBox);
            EditorGUILayout.LabelField("Native Status",
                mat.IsRegistered
                    ? $"Registered (ID: {mat.NativeMaterialID})"
                    : "Not Registered",
                EditorStyles.miniLabel);
            EditorGUILayout.EndHorizontal();
        }
    }
}
