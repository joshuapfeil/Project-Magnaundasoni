// ============================================================================
// DemoSceneManager.cs – Demo scene controller with stats overlay
// ============================================================================
using UnityEngine;
using Magnaundasoni;

namespace MagnaundasoniDemo
{
    /// <summary>
    /// Controls demo scenarios: toggles quality levels, switches between
    /// BuiltIn / Integration rendering modes, and displays a real-time
    /// performance stats overlay via OnGUI.
    /// </summary>
    public class DemoSceneManager : MonoBehaviour
    {
        [Header("Key Bindings")]
        [SerializeField] private KeyCode _toggleOverlayKey = KeyCode.F1;
        [SerializeField] private KeyCode _cycleQualityKey = KeyCode.F2;
        [SerializeField] private KeyCode _toggleModeKey = KeyCode.F3;
        [SerializeField] private KeyCode _nextScenarioKey = KeyCode.F4;

        [Header("Demo Scenarios")]
        [Tooltip("Root GameObjects for each scenario. Only one is active at a time.")]
        [SerializeField] private GameObject[] _scenarios;

        private bool _showOverlay = true;
        private int _currentScenarioIndex;
        private MagQualityLevel _currentQuality = MagQualityLevel.Medium;
        private GUIStyle _boxStyle;
        private GUIStyle _labelStyle;
        private GUIStyle _headerStyle;

        private void Start()
        {
            ActivateScenario(_currentScenarioIndex);
        }

        private void Update()
        {
            if (Input.GetKeyDown(_toggleOverlayKey))
                _showOverlay = !_showOverlay;

            if (Input.GetKeyDown(_cycleQualityKey))
                CycleQuality();

            if (Input.GetKeyDown(_toggleModeKey))
                ToggleRenderingMode();

            if (Input.GetKeyDown(_nextScenarioKey))
                NextScenario();
        }

        // ----- Quality Cycling --------------------------------------------
        private void CycleQuality()
        {
            int next = ((int)_currentQuality + 1) % 4;
            _currentQuality = (MagQualityLevel)next;

            var engine = MagnaundasoniEngine.Instance;
            if (engine != null && engine.IsInitialized)
                engine.SetQuality(_currentQuality);
        }

        // ----- Rendering Mode Toggle --------------------------------------
        private void ToggleRenderingMode()
        {
            var engine = MagnaundasoniEngine.Instance;
            if (engine == null) return;

            RenderingMode newMode = engine.CurrentMode == RenderingMode.Integration
                ? RenderingMode.BuiltIn
                : RenderingMode.Integration;
            engine.SetRenderingMode(newMode);
        }

        // ----- Scenario Control -------------------------------------------
        private void NextScenario()
        {
            if (_scenarios == null || _scenarios.Length == 0) return;
            _currentScenarioIndex = (_currentScenarioIndex + 1) % _scenarios.Length;
            ActivateScenario(_currentScenarioIndex);
        }

        private void ActivateScenario(int index)
        {
            if (_scenarios == null) return;
            for (int i = 0; i < _scenarios.Length; i++)
            {
                if (_scenarios[i] != null)
                    _scenarios[i].SetActive(i == index);
            }
        }

        // ----- OnGUI Performance Overlay ----------------------------------
        private void OnGUI()
        {
            if (!_showOverlay) return;

            EnsureStyles();

            float width = 300f;
            float height = 220f;
            Rect area = new Rect(10, 10, width, height);

            GUI.Box(area, GUIContent.none, _boxStyle);
            GUILayout.BeginArea(new Rect(area.x + 8, area.y + 8,
                area.width - 16, area.height - 16));

            GUILayout.Label("Magnaundasoni Stats", _headerStyle);
            GUILayout.Space(4);

            var engine = MagnaundasoniEngine.Instance;
            if (engine != null && engine.IsInitialized)
            {
                MagGlobalState state = engine.GlobalState;

                DrawStatLine("Quality", state.activeQuality.ToString());
                DrawStatLine("Backend", state.backendUsed.ToString());
                DrawStatLine("Mode", engine.CurrentMode.ToString());
                DrawStatLine("Active Sources", state.activeSourceCount.ToString());
                DrawStatLine("CPU Time", $"{state.cpuTimeMs:F2} ms");
                DrawStatLine("Ray Count", engine.GetDebugRayCount().ToString());
                DrawStatLine("Active Edges", engine.GetDebugActiveEdges().ToString());
                DrawStatLine("FPS", $"{1f / Time.unscaledDeltaTime:F0}");
            }
            else
            {
                GUILayout.Label("Engine not initialized.", _labelStyle);
            }

            GUILayout.Space(4);
            GUILayout.Label(
                "[F1] Overlay  [F2] Quality  [F3] Mode  [F4] Scenario",
                _labelStyle);

            GUILayout.EndArea();
        }

        private void DrawStatLine(string label, string value)
        {
            GUILayout.BeginHorizontal();
            GUILayout.Label(label, _labelStyle, GUILayout.Width(120));
            GUILayout.Label(value, _labelStyle);
            GUILayout.EndHorizontal();
        }

        private void EnsureStyles()
        {
            if (_boxStyle != null) return;

            _boxStyle = new GUIStyle(GUI.skin.box);
            Texture2D bgTex = new Texture2D(1, 1);
            bgTex.SetPixel(0, 0, new Color(0f, 0f, 0f, 0.75f));
            bgTex.Apply();
            _boxStyle.normal.background = bgTex;

            _labelStyle = new GUIStyle(GUI.skin.label)
            {
                fontSize = 12,
                normal = { textColor = Color.white }
            };

            _headerStyle = new GUIStyle(_labelStyle)
            {
                fontSize = 14,
                fontStyle = FontStyle.Bold,
                normal = { textColor = new Color(0.3f, 0.9f, 1f) }
            };
        }
    }
}
