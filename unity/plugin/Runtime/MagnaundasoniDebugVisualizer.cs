// ============================================================================
// MagnaundasoniDebugVisualizer.cs – Debug visualization overlay
// ============================================================================
using System;
using System.Collections.Generic;
using UnityEngine;

namespace Magnaundasoni
{
    [AddComponentMenu("Magnaundasoni/Debug Visualizer")]
    public class MagnaundasoniDebugVisualizer : MonoBehaviour
    {
        // ----- Inspector ---------------------------------------------------
        [Header("Visualization Toggles")]
        [SerializeField] private bool _showDirectPaths = true;
        [SerializeField] private bool _showReflections = true;
        [SerializeField] private bool _showDiffractions = true;
        [SerializeField] private bool _showEdges = true;
        [SerializeField] private bool _showPerBandEnergy = false;

        [Header("Colors")]
        [SerializeField] private Color _directColor = Color.green;
        [SerializeField] private Color _reflectionColor = Color.yellow;
        [SerializeField] private Color _diffractionColor = Color.magenta;
        [SerializeField] private Color _edgeColor = new Color(1f, 0.5f, 0f, 0.6f);

        [Header("Display")]
        [SerializeField] [Range(0.5f, 5f)] private float _lineWidth = 1.5f;
        [SerializeField] private bool _showStatsOverlay = true;

        // ----- Band Colors for Energy Visualization ------------------------
        private static readonly Color[] BandColors = new Color[]
        {
            new Color(1f, 0f, 0f, 0.8f),       // 63 Hz   – red
            new Color(1f, 0.4f, 0f, 0.8f),     // 125 Hz  – orange
            new Color(1f, 1f, 0f, 0.8f),       // 250 Hz  – yellow
            new Color(0.5f, 1f, 0f, 0.8f),     // 500 Hz  – lime
            new Color(0f, 1f, 0f, 0.8f),       // 1 kHz   – green
            new Color(0f, 1f, 1f, 0.8f),       // 2 kHz   – cyan
            new Color(0f, 0.5f, 1f, 0.8f),     // 4 kHz   – blue
            new Color(0.5f, 0f, 1f, 0.8f),     // 8 kHz   – violet
        };

        private static readonly string[] BandLabels = new string[]
        {
            "63 Hz", "125 Hz", "250 Hz", "500 Hz",
            "1 kHz", "2 kHz", "4 kHz", "8 kHz"
        };

        // ----- Runtime Toggle ----------------------------------------------
        private bool _enabled = true;
        public bool VisualizationEnabled
        {
            get => _enabled;
            set => _enabled = value;
        }

        // ----- Lifecycle ---------------------------------------------------
        private void OnEnable() { _enabled = true; }
        private void OnDisable() { _enabled = false; }

        // ----- GL-Based Rendering (Game View) ------------------------------
        private void OnPostRender()
        {
            if (!_enabled) return;
            DrawVisualization();
        }

        private void OnRenderObject()
        {
            if (!_enabled) return;
            if (Camera.current != Camera.main) return;
            DrawVisualization();
        }

        private void DrawVisualization()
        {
            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized) return;

            var listener = MagnaundasoniListener.ActiveListener;
            if (listener == null) return;

            GL.PushMatrix();
            GL.MultMatrix(Matrix4x4.identity);

#if UNITY_2022_2_OR_NEWER
            var sources = Object.FindObjectsByType<MagnaundasoniSource>(FindObjectsSortMode.None);
#else
            var sources = Object.FindObjectsOfType<MagnaundasoniSource>();
#endif
            foreach (var source in sources)
            {
                if (!source.IsRegistered) continue;
                var result = source.LastResult;
                DrawSourceVisualization(source.transform.position, listener.transform.position, result);
            }

            GL.PopMatrix();
        }

        private void DrawSourceVisualization(Vector3 srcPos, Vector3 listenerPos,
            MagAcousticResult result)
        {
            CreateLineMaterial();

            _lineMaterial.SetPass(0);
            GL.Begin(GL.LINES);

            if (_showDirectPaths && result.direct.direction != null)
            {
                GL.Color(_directColor);
                GL.Vertex(srcPos);
                GL.Vertex(listenerPos);
            }

            if (_showReflections)
            {
                var reflections = result.GetReflections();
                for (int i = 0; i < reflections.Length; i++)
                {
                    float alpha = Mathf.Clamp01(1f - (float)reflections[i].order / 5f);
                    Color col = _reflectionColor;
                    col.a = alpha;
                    GL.Color(col);

                    Vector3 dir = new Vector3(
                        reflections[i].direction[0],
                        reflections[i].direction[1],
                        reflections[i].direction[2]);

                    GL.Vertex(listenerPos);
                    GL.Vertex(listenerPos + dir * 2f);
                }
            }

            if (_showDiffractions)
            {
                var diffractions = result.GetDiffractions();
                for (int i = 0; i < diffractions.Length; i++)
                {
                    GL.Color(_diffractionColor);
                    Vector3 dir = new Vector3(
                        diffractions[i].direction[0],
                        diffractions[i].direction[1],
                        diffractions[i].direction[2]);

                    GL.Vertex(listenerPos);
                    GL.Vertex(listenerPos + dir * 2f);
                }
            }

            GL.End();
        }

        // ----- Per-Band Energy Overlay -------------------------------------
        private void DrawPerBandOverlay(Vector3 position, float[] bandEnergy)
        {
            if (!_showPerBandEnergy || bandEnergy == null) return;

            CreateLineMaterial();
            _lineMaterial.SetPass(0);
            GL.Begin(GL.QUADS);

            float barWidth = 0.05f;
            float maxHeight = 1f;

            for (int i = 0; i < Mathf.Min(bandEnergy.Length, MagConstants.MaxBands); i++)
            {
                float height = Mathf.Clamp01(bandEnergy[i]) * maxHeight;
                float x = position.x + i * barWidth * 1.5f;
                GL.Color(BandColors[i]);

                GL.Vertex3(x, position.y, position.z);
                GL.Vertex3(x + barWidth, position.y, position.z);
                GL.Vertex3(x + barWidth, position.y + height, position.z);
                GL.Vertex3(x, position.y + height, position.z);
            }

            GL.End();
        }

        // ----- Stats Overlay (OnGUI) ---------------------------------------
        private void OnGUI()
        {
            if (!_enabled || !_showStatsOverlay) return;

            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized) return;

            MagGlobalState state = engine.GlobalState;
            uint rayCount = engine.GetDebugRayCount();
            uint edgeCount = engine.GetDebugActiveEdges();

            float x = 10f;
            float y = 10f;
            float w = 280f;
            float lineH = 18f;

            GUI.Box(new Rect(x, y, w, lineH * 7 + 10), "");

            GUI.color = Color.white;
            GUI.Label(new Rect(x + 5, y, w, lineH), "<b>Magnaundasoni Debug</b>");
            y += lineH;
            GUI.Label(new Rect(x + 5, y, w, lineH),
                $"Quality: {state.activeQuality}  Backend: {state.backendUsed}");
            y += lineH;
            GUI.Label(new Rect(x + 5, y, w, lineH),
                $"Active Sources: {state.activeSourceCount}");
            y += lineH;
            GUI.Label(new Rect(x + 5, y, w, lineH),
                $"CPU Time: {state.cpuTimeMs:F2} ms");
            y += lineH;
            GUI.Label(new Rect(x + 5, y, w, lineH),
                $"Rays: {rayCount}  Edges: {edgeCount}");
            y += lineH;
            GUI.Label(new Rect(x + 5, y, w, lineH),
                $"Mode: {engine.CurrentMode}");
            y += lineH;
            GUI.Label(new Rect(x + 5, y, w, lineH),
                $"Timestamp: {state.timestamp:F3}");
        }

        // ----- Line Material -----------------------------------------------
        private static Material _lineMaterial;

        private static void CreateLineMaterial()
        {
            if (_lineMaterial != null) return;

            Shader shader = Shader.Find("Hidden/Internal-Colored");
            _lineMaterial = new Material(shader)
            {
                hideFlags = HideFlags.HideAndDontSave
            };
            _lineMaterial.SetInt("_SrcBlend", (int)UnityEngine.Rendering.BlendMode.SrcAlpha);
            _lineMaterial.SetInt("_DstBlend", (int)UnityEngine.Rendering.BlendMode.OneMinusSrcAlpha);
            _lineMaterial.SetInt("_Cull", (int)UnityEngine.Rendering.CullMode.Off);
            _lineMaterial.SetInt("_ZWrite", 0);
        }

        // ----- Gizmos (Scene View) -----------------------------------------
#if UNITY_EDITOR
        private void OnDrawGizmos()
        {
            if (!_enabled) return;

            var engine = MagnaundasoniEngine.Instance;
            if (engine == null || !engine.IsInitialized) return;

            var listener = MagnaundasoniListener.ActiveListener;
            if (listener == null) return;

#if UNITY_2022_2_OR_NEWER
            var sources = Object.FindObjectsByType<MagnaundasoniSource>(FindObjectsSortMode.None);
#else
            var sources = Object.FindObjectsOfType<MagnaundasoniSource>();
#endif
            foreach (var source in sources)
            {
                if (!source.IsRegistered) continue;
                var result = source.LastResult;

                if (_showDirectPaths && result.direct.direction != null)
                {
                    Gizmos.color = _directColor;
                    Gizmos.DrawLine(source.transform.position, listener.transform.position);
                }

                if (_showReflections)
                {
                    var reflections = result.GetReflections();
                    foreach (var tap in reflections)
                    {
                        float alpha = Mathf.Clamp01(1f - (float)tap.order / 5f);
                        Gizmos.color = new Color(
                            _reflectionColor.r, _reflectionColor.g, _reflectionColor.b, alpha);
                        Vector3 dir = new Vector3(tap.direction[0], tap.direction[1], tap.direction[2]);
                        Gizmos.DrawRay(listener.transform.position, dir * 2f);
                    }
                }

                if (_showDiffractions)
                {
                    var diffractions = result.GetDiffractions();
                    foreach (var tap in diffractions)
                    {
                        Gizmos.color = _diffractionColor;
                        Vector3 dir = new Vector3(tap.direction[0], tap.direction[1], tap.direction[2]);
                        Gizmos.DrawRay(listener.transform.position, dir * 2f);
                    }
                }
            }
        }
#endif
    }
}
