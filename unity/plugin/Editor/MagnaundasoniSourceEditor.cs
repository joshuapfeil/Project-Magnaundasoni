// ============================================================================
// MagnaundasoniSourceEditor.cs – Custom editor for MagnaundasoniSource
// ============================================================================
using UnityEngine;
using UnityEditor;

namespace Magnaundasoni
{
    [CustomEditor(typeof(MagnaundasoniSource))]
    public class MagnaundasoniSourceEditor : Editor
    {
        private static readonly string[] BandLabels = new string[]
        {
            "63 Hz", "125 Hz", "250 Hz", "500 Hz",
            "1 kHz", "2 kHz", "4 kHz", "8 kHz"
        };

        private bool _showDirect = true;
        private bool _showReflections = true;
        private bool _showLateField = true;

        public override void OnInspectorGUI()
        {
            DrawDefaultInspector();

            var source = (MagnaundasoniSource)target;

            EditorGUILayout.Space(8);
            EditorGUILayout.LabelField("Runtime Status", EditorStyles.boldLabel);

            if (!Application.isPlaying)
            {
                EditorGUILayout.HelpBox(
                    "Enter Play Mode to view real-time acoustic data.",
                    MessageType.Info);
                return;
            }

            DrawRegistrationInfo(source);

            if (!source.IsRegistered) return;

            MagAcousticResult result = source.LastResult;

            DrawDirectComponent(result.direct);
            DrawReflectionsSummary(result);
            DrawLateFieldInfo(result.lateField);

            // Force continuous repaint during play mode
            Repaint();
        }

        private void DrawRegistrationInfo(MagnaundasoniSource source)
        {
            EditorGUILayout.BeginVertical(EditorStyles.helpBox);
            EditorGUILayout.LabelField("Registered",
                source.IsRegistered ? "Yes" : "No");
            if (source.IsRegistered)
                EditorGUILayout.LabelField("Native Source ID",
                    source.NativeSourceID.ToString());
            EditorGUILayout.EndVertical();
        }

        private void DrawDirectComponent(MagDirectComponent direct)
        {
            _showDirect = EditorGUILayout.BeginFoldoutHeaderGroup(_showDirect,
                "Direct Path");
            if (_showDirect)
            {
                EditorGUI.indentLevel++;

                EditorGUILayout.LabelField("Delay",
                    $"{direct.delay * 1000f:F2} ms");
                EditorGUILayout.LabelField("Occlusion LPF",
                    direct.occlusionLPF > 0f
                        ? $"{direct.occlusionLPF:F0} Hz"
                        : "None");
                EditorGUILayout.LabelField("Confidence",
                    $"{direct.confidence:P0}");

                if (direct.direction != null && direct.direction.Length == 3)
                {
                    Vector3 dir = new Vector3(
                        direct.direction[0], direct.direction[1], direct.direction[2]);
                    EditorGUILayout.LabelField("Direction", dir.ToString("F2"));
                }

                EditorGUILayout.Space(4);
                EditorGUILayout.LabelField("Per-Band Gain", EditorStyles.miniLabel);
                DrawBandBars(direct.perBandGain, new Color(0.2f, 0.7f, 1f));

                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
        }

        private void DrawReflectionsSummary(MagAcousticResult result)
        {
            _showReflections = EditorGUILayout.BeginFoldoutHeaderGroup(
                _showReflections, "Reflections & Diffraction");
            if (_showReflections)
            {
                EditorGUI.indentLevel++;
                EditorGUILayout.LabelField("Reflection Taps",
                    result.reflectionCount.ToString());
                EditorGUILayout.LabelField("Diffraction Paths",
                    result.diffractionCount.ToString());
                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
        }

        private void DrawLateFieldInfo(MagLateField lateField)
        {
            _showLateField = EditorGUILayout.BeginFoldoutHeaderGroup(
                _showLateField, "Late Field / Reverb");
            if (_showLateField)
            {
                EditorGUI.indentLevel++;

                EditorGUILayout.LabelField("Room Size Estimate",
                    $"{lateField.roomSizeEstimate:F1} m");
                EditorGUILayout.LabelField("Diffuse Directionality",
                    $"{lateField.diffuseDirectionality:F2}");

                EditorGUILayout.Space(4);
                EditorGUILayout.LabelField("RT60 (per band)", EditorStyles.miniLabel);
                DrawBandBars(lateField.rt60, new Color(1f, 0.8f, 0.2f), 5f);

                EditorGUILayout.Space(2);
                EditorGUILayout.LabelField("Decay (per band)", EditorStyles.miniLabel);
                DrawBandBars(lateField.perBandDecay, new Color(0.9f, 0.4f, 0.4f));

                EditorGUI.indentLevel--;
            }
            EditorGUILayout.EndFoldoutHeaderGroup();
        }

        /// <summary>
        /// Draws horizontal bar meters for each frequency band.
        /// </summary>
        private void DrawBandBars(float[] values, Color barColor, float maxValue = 1f)
        {
            if (values == null || values.Length == 0) return;

            int count = Mathf.Min(values.Length, MagConstants.MaxBands);
            for (int i = 0; i < count; i++)
            {
                EditorGUILayout.BeginHorizontal();
                EditorGUILayout.LabelField(BandLabels[i], GUILayout.Width(50));

                Rect barRect = GUILayoutUtility.GetRect(GUIContent.none, GUIStyle.none,
                    GUILayout.Height(14), GUILayout.ExpandWidth(true));

                if (Event.current.type == EventType.Repaint)
                {
                    EditorGUI.DrawRect(barRect, new Color(0.2f, 0.2f, 0.2f));
                    float fill = Mathf.Clamp01(values[i] / maxValue);
                    Rect fillRect = new Rect(barRect.x, barRect.y,
                        barRect.width * fill, barRect.height);
                    EditorGUI.DrawRect(fillRect, barColor);
                }

                EditorGUILayout.LabelField(values[i].ToString("F3"),
                    GUILayout.Width(50));
                EditorGUILayout.EndHorizontal();
            }
        }
    }
}
