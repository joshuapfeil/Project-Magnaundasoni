// ============================================================================
// MagnaundasoniEngine.cs – High-level Unity singleton for the acoustics engine
// ============================================================================
using System;
using System.Collections.Generic;
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
#if UNITY_2022_2_OR_NEWER
                    _instance = FindFirstObjectByType<MagnaundasoniEngine>();
#else
                    _instance = FindObjectOfType<MagnaundasoniEngine>();
#endif
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

        public static MagnaundasoniEngine Current => _instance;

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

        [Header("Auto Registration")]
        [Tooltip("Automatically scan and register all scene geometry on startup.")]
        [SerializeField] private bool _autoRegisterSceneGeometry = true;
        [Tooltip("Automatically register geometry when new scenes are loaded.")]
        [SerializeField] private bool _autoRegisterOnSceneLoad = true;
        [Tooltip("Default material preset for auto-registered geometry.")]
        [SerializeField] private string _defaultMaterialPreset = "General";

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
            if (_autoRegisterSceneGeometry && IsInitialized)
                AutoRegisterAllSceneGeometry();
            if (_autoRegisterOnSceneLoad)
                UnityEngine.SceneManagement.SceneManager.sceneLoaded += OnSceneLoaded;
        }

        private void OnDisable()
        {
            if (_autoRegisterOnSceneLoad)
                UnityEngine.SceneManagement.SceneManager.sceneLoaded -= OnSceneLoaded;
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
                MagnaundasoniNative.mag_set_unity_graphics_renderer((int)SystemInfo.graphicsDeviceType);
                _engineHandle = MagAPI.EngineCreate(config);
                MagAPI.BindUnityGraphicsDevice(_engineHandle);
                var diagnostics = MagAPI.GetBackendDiagnostics(_engineHandle);
                Debug.Log("[Magnaundasoni] Engine initialized " +
                    $"(quality={_quality}, requestedBackend={_backend}, activeBackend={diagnostics.activeBackend}, " +
                    $"computeEnabled={diagnostics.computeEnabled != 0}, externalD3D11={diagnostics.usingExternalD3D11Device != 0}, bands={_effectiveBandCount})");
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

        // ----- Auto Registration -------------------------------------------
        private readonly HashSet<int> _autoRegisteredObjects = new HashSet<int>();

        private void OnSceneLoaded(UnityEngine.SceneManagement.Scene scene,
            UnityEngine.SceneManagement.LoadSceneMode mode)
        {
            if (!IsInitialized || !_autoRegisterOnSceneLoad) return;
            AutoRegisterSceneGeometry(scene);
        }

        /// <summary>
        /// Scans ALL loaded scenes and registers every MeshFilter that does
        /// not already have a MagnaundasoniGeometry override component.
        /// </summary>
        public void AutoRegisterAllSceneGeometry()
        {
            for (int i = 0; i < UnityEngine.SceneManagement.SceneManager.sceneCount; i++)
            {
                var scene = UnityEngine.SceneManagement.SceneManager.GetSceneAt(i);
                if (scene.isLoaded)
                    AutoRegisterSceneGeometry(scene);
            }
        }

        private void AutoRegisterSceneGeometry(UnityEngine.SceneManagement.Scene scene)
        {
            var roots = scene.GetRootGameObjects();
            foreach (var root in roots)
            {
                var meshFilters = root.GetComponentsInChildren<MeshFilter>(false);
                foreach (var mf in meshFilters)
                {
                    // Skip objects that have an explicit MagnaundasoniGeometry component
                    if (mf.GetComponent<MagnaundasoniGeometry>() != null) continue;

                    // Skip objects already auto-registered
                    int instanceID = mf.gameObject.GetInstanceID();
                    if (_autoRegisteredObjects.Contains(instanceID)) continue;

                    // Skip objects with no mesh
                    if (mf.sharedMesh == null) continue;

                    // Skip objects with no renderer (invisible objects)
                    var renderer = mf.GetComponent<MeshRenderer>();
                    if (renderer == null || !renderer.enabled) continue;

                    // Determine dynamic flag based on static flags and rigidbody
                    uint dynamicFlag = (uint)MagDynamicFlag.Static;
                    if (!mf.gameObject.isStatic)
                    {
                        var rb = mf.GetComponent<Rigidbody>();
                        dynamicFlag = rb != null
                            ? (uint)MagDynamicFlag.DynamicImportant
                            : (uint)MagDynamicFlag.DynamicMinor;
                    }

                    // Register via the native API
                    RegisterMeshGeometry(mf, dynamicFlag);
                    _autoRegisteredObjects.Add(instanceID);
                }
            }

            int count = _autoRegisteredObjects.Count;
            if (count > 0)
                Debug.Log($"[Magnaundasoni] Auto-registered {count} geometry objects from scene '{scene.name}'");
        }

        private unsafe void RegisterMeshGeometry(MeshFilter mf, uint dynamicFlag)
        {
            Mesh mesh = mf.sharedMesh;
            Vector3[] meshVertices = mesh.vertices;
            int[] meshTriangles = mesh.triangles;

            float[] worldVertices = new float[meshVertices.Length * 3];
            Matrix4x4 ltw = mf.transform.localToWorldMatrix;
            for (int i = 0; i < meshVertices.Length; i++)
            {
                Vector3 wp = ltw.MultiplyPoint3x4(meshVertices[i]);
                worldVertices[i * 3 + 0] = wp.x;
                worldVertices[i * 3 + 1] = wp.y;
                worldVertices[i * 3 + 2] = wp.z;
            }

            uint[] indices = new uint[meshTriangles.Length];
            for (int i = 0; i < meshTriangles.Length; i++)
                indices[i] = (uint)meshTriangles[i];

            fixed (float* vertPtr = worldVertices)
            fixed (uint* idxPtr = indices)
            {
                var desc = new MagGeometryDesc
                {
                    vertices    = vertPtr,
                    vertexCount = (uint)meshVertices.Length,
                    indices     = idxPtr,
                    indexCount  = (uint)indices.Length,
                    materialID  = 0, // Default material
                    dynamicFlag = dynamicFlag
                };

                try
                {
                    MagAPI.GeometryRegister(_engineHandle, desc);
                }
                catch (MagnaundasoniException ex)
                {
                    Debug.LogWarning($"[Magnaundasoni] Auto-register failed for '{mf.gameObject.name}': {ex.Message}");
                }
            }
        }
    }
}
