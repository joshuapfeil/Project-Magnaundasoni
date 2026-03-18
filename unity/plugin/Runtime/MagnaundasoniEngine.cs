// ============================================================================
// MagnaundasoniEngine.cs – High-level Unity singleton for the acoustics engine
// ============================================================================
using System;
using System.Threading;
using UnityEngine;

namespace Magnaundasoni
{
    public enum RenderingMode
    {
        BuiltIn,
        Integration
    }

    [AddComponentMenu("Magnaundasoni/Engine")]
    [DefaultExecutionOrder(-1000)]
    public class MagnaundasoniEngine : MonoBehaviour
    {
        // ----- Singleton ---------------------------------------------------
        private static MagnaundasoniEngine _instance;
        private static readonly object _lock = new object();

        public static MagnaundasoniEngine Instance
        {
            get
            {
                if (_instance == null)
                {
                    _instance = FindObjectOfType<MagnaundasoniEngine>();
                    if (_instance == null)
                    {
                        var go = new GameObject("[MagnaundasoniEngine]");
                        _instance = go.AddComponent<MagnaundasoniEngine>();
                        DontDestroyOnLoad(go);
                    }
                }
                return _instance;
            }
        }

        // ----- Inspector Configuration ------------------------------------
        [Header("Quality")]
        [Tooltip("Acoustic simulation quality level.")]
        [SerializeField] private MagQualityLevel _quality = MagQualityLevel.Medium;

        [Tooltip("Preferred ray-tracing backend.")]
        [SerializeField] private MagBackendType _backend = MagBackendType.Auto;

        [Header("Limits")]
        [SerializeField] private uint _maxSources = 64;
        [SerializeField] private uint _maxReflectionOrder = 4;
        [SerializeField] private uint _maxDiffractionDepth = 2;
        [SerializeField] private uint _raysPerSource = 512;

        [Header("Threading")]
        [SerializeField] private uint _threadCount = 0; // 0 = auto

        [Header("World")]
        [SerializeField] private float _worldChunkSize = 32f;
        [Range(4, 8)]
        [SerializeField] private uint _effectiveBandCount = 8;

        [Header("Mode")]
        [Tooltip("BuiltIn: engine handles audio output. Integration: maps results to Unity AudioSources.")]
        [SerializeField] private RenderingMode _renderingMode = RenderingMode.Integration;

        // ----- Public Properties ------------------------------------------
        public IntPtr NativeHandle => _engineHandle;
        public bool IsInitialized => _engineHandle != IntPtr.Zero;
        public RenderingMode CurrentMode => _renderingMode;
        public MagQualityLevel Quality => _quality;
        public MagBackendType Backend => _backend;

        // ----- Thread-safe result cache -----------------------------------
        private MagGlobalState _cachedGlobalState;
        private readonly object _stateLock = new object();

        public MagGlobalState GlobalState
        {
            get { lock (_stateLock) { return _cachedGlobalState; } }
        }

        // ----- Private State -----------------------------------------------
        private IntPtr _engineHandle = IntPtr.Zero;

        // ----- Lifecycle ---------------------------------------------------
        private void Awake()
        {
            if (_instance != null && _instance != this)
            {
                Destroy(gameObject);
                return;
            }
            _instance = this;
            DontDestroyOnLoad(gameObject);
        }

        private void OnEnable()
        {
            InitializeEngine();
        }

        private void OnDisable()
        {
            ShutdownEngine();
        }

        private void OnDestroy()
        {
            ShutdownEngine();
            if (_instance == this) _instance = null;
        }

        private void LateUpdate()
        {
            if (!IsInitialized) return;

            try
            {
                MagAPI.Update(_engineHandle, Time.deltaTime);
                var state = MagAPI.GetGlobalState(_engineHandle);
                lock (_stateLock) { _cachedGlobalState = state; }
            }
            catch (MagnaundasoniException ex)
            {
                Debug.LogError($"[Magnaundasoni] Frame update failed: {ex.Message}");
            }
        }

        // ----- Public Methods ----------------------------------------------
        public void SetQuality(MagQualityLevel level)
        {
            _quality = level;
            if (IsInitialized)
            {
                try { MagAPI.SetQuality(_engineHandle, level); }
                catch (MagnaundasoniException ex)
                {
                    Debug.LogError($"[Magnaundasoni] SetQuality failed: {ex.Message}");
                }
            }
        }

        public void SetRenderingMode(RenderingMode mode)
        {
            _renderingMode = mode;
        }

        public MagAcousticResult QueryResult(uint sourceID, uint listenerID)
        {
            if (!IsInitialized)
                return default;

            try
            {
                return MagAPI.GetAcousticResult(_engineHandle, sourceID, listenerID);
            }
            catch (MagnaundasoniException)
            {
                return default;
            }
        }

        public uint GetDebugRayCount()
        {
            if (!IsInitialized) return 0;
            try { return MagAPI.DebugGetRayCount(_engineHandle); }
            catch { return 0; }
        }

        public uint GetDebugActiveEdges()
        {
            if (!IsInitialized) return 0;
            try { return MagAPI.DebugGetActiveEdges(_engineHandle); }
            catch { return 0; }
        }

        // ----- Internal Helpers --------------------------------------------
        private void InitializeEngine()
        {
            if (_engineHandle != IntPtr.Zero) return;

            var config = new MagEngineConfig
            {
                quality             = _quality,
                preferredBackend    = _backend,
                maxSources          = _maxSources,
                maxReflectionOrder  = _maxReflectionOrder,
                maxDiffractionDepth = _maxDiffractionDepth,
                raysPerSource       = _raysPerSource,
                threadCount         = _threadCount,
                worldChunkSize      = _worldChunkSize,
                effectiveBandCount  = _effectiveBandCount
            };

            try
            {
                _engineHandle = MagAPI.EngineCreate(config);
                Debug.Log("[Magnaundasoni] Engine initialized " +
                    $"(quality={_quality}, backend={_backend}, bands={_effectiveBandCount})");
            }
            catch (MagnaundasoniException ex)
            {
                Debug.LogError($"[Magnaundasoni] Failed to create engine: {ex.Message}");
                _engineHandle = IntPtr.Zero;
            }
        }

        private void ShutdownEngine()
        {
            if (_engineHandle == IntPtr.Zero) return;

            try
            {
                MagAPI.EngineDestroy(_engineHandle);
                Debug.Log("[Magnaundasoni] Engine shut down.");
            }
            catch (MagnaundasoniException ex)
            {
                Debug.LogWarning($"[Magnaundasoni] Engine shutdown error: {ex.Message}");
            }
            finally
            {
                _engineHandle = IntPtr.Zero;
            }
        }
    }
}
