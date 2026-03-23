// ============================================================================
// MagnaundasoniSource.cs – Acoustic source component
// ============================================================================
using System;
using UnityEngine;

namespace Magnaundasoni
{
    [DisallowMultipleComponent]
    [AddComponentMenu("Magnaundasoni/Acoustic Source")]
    [RequireComponent(typeof(AudioSource))]
    public class MagnaundasoniSource : MonoBehaviour
    {
        // ----- Inspector ---------------------------------------------------
        [Header("Acoustic Properties")]
        [Tooltip("Importance of this source for simulation budget.")]
        [SerializeField] private MagImportance _importance = MagImportance.Medium;

        [Tooltip("Near-field radius in meters.")]
        [SerializeField] [Range(0.01f, 10f)] private float _radius = 0.1f;

        [Header("Integration Mode Settings")]
        [Tooltip("Controls how aggressively occlusion maps to volume.")]
        [SerializeField] [Range(0f, 1f)] private float _occlusionWeight = 0.8f;

        [Tooltip("Map late-field to Unity reverb zone mix.")]
        [SerializeField] [Range(0f, 1f)] private float _reverbMixWeight = 0.5f;

        // ----- Public Properties -------------------------------------------
        public uint NativeSourceID => _sourceID;
        public bool IsRegistered => _registered;
        public MagAcousticResult LastResult => _lastResult;

        // ----- Private State -----------------------------------------------
        private uint _sourceID;
        private bool _registered;
        private MagAcousticResult _lastResult;
        private AudioSource _audioSource;

        // ----- Lifecycle ---------------------------------------------------
        private void OnEnable()
        {
            _audioSource = GetComponent<AudioSource>();
            Register();
        }

        private void OnDisable()
        {
            Unregister();
        }

        private void Update()
        {
            if (!_registered) return;

            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized) return;

            UpdateNativePosition(engine);
            if (engine.CurrentMode == RenderingMode.Integration)
                FetchAndApplyResults(engine);
        }

        private void OnAudioFilterRead(float[] data, int channels)
        {
            if (!_registered || data == null || data.Length == 0 || channels <= 0) return;

            var engine = MagnaundasoniEngine.Current;
            if (engine == null || !engine.IsInitialized) return;
            if (engine.CurrentMode != RenderingMode.BuiltIn) return;

            try
            {
                MagAPI.SubmitSourceAudio(engine.NativeHandle, _sourceID,
                    data, (uint)(data.Length / channels), (uint)channels);
                Array.Clear(data, 0, data.Length);
            }
            catch (MagnaundasoniException)
            {
            }
        }

        // ----- Registration ------------------------------------------------
        private void Register()
        {
            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized) return;

            var desc = BuildSourceDesc();
            try
            {
                _sourceID = MagAPI.SourceRegister(engine.NativeHandle, desc);
                _registered = true;
            }
            catch (MagnaundasoniException ex)
            {
                Debug.LogError($"[Magnaundasoni] Source register failed: {ex.Message}");
            }
        }

        private void Unregister()
        {
            if (!_registered) return;

            var engine = MagnaundasoniEngine.Instance;
            if (engine != null && engine.IsInitialized)
            {
                try { MagAPI.SourceUnregister(engine.NativeHandle, _sourceID); }
                catch (MagnaundasoniException) { }
            }
            _registered = false;
        }

        // ----- Per-Frame Updates -------------------------------------------
        private void UpdateNativePosition(MagnaundasoniEngine engine)
        {
            var desc = BuildSourceDesc();
            try { MagAPI.SourceUpdate(engine.NativeHandle, _sourceID, desc); }
            catch (MagnaundasoniException) { }
        }

        private void FetchAndApplyResults(MagnaundasoniEngine engine)
        {
            var listener = MagnaundasoniListener.ActiveListener;
            if (listener == null || !listener.IsRegistered) return;

            _lastResult = engine.QueryResult(_sourceID, listener.NativeListenerID);

            if (engine.CurrentMode == RenderingMode.Integration)
                ApplyIntegrationMode();
        }

        private void ApplyIntegrationMode()
        {
            if (_audioSource == null) return;

            float avgGain = ComputeAverageGain(_lastResult.direct.perBandGain);
            float occlusionFactor = _lastResult.direct.occlusionLPF > 0f
                ? Mathf.Clamp01(1f - (_lastResult.direct.occlusionLPF / 22000f))
                : 0f;

            _audioSource.volume = Mathf.Clamp01(avgGain * (1f - occlusionFactor * _occlusionWeight));
            _audioSource.spatialBlend = 1f;

            float avgRT60 = ComputeAverage(_lastResult.lateField.rt60);
            float reverbZoneMix = Mathf.Clamp01(avgRT60 * _reverbMixWeight);
            _audioSource.reverbZoneMix = reverbZoneMix;

            if (_lastResult.direct.direction != null && _lastResult.direct.direction.Length == 3)
            {
                Vector3 dir = new Vector3(
                    _lastResult.direct.direction[0],
                    _lastResult.direct.direction[1],
                    _lastResult.direct.direction[2]);
                if (dir.sqrMagnitude > 0.001f)
                    _audioSource.transform.rotation = Quaternion.LookRotation(dir);
            }
        }

        // ----- Helpers -----------------------------------------------------
        private MagSourceDesc BuildSourceDesc()
        {
            Vector3 pos = transform.position;
            Vector3 fwd = transform.forward;
            return new MagSourceDesc
            {
                position  = new float[] { pos.x, pos.y, pos.z },
                direction = new float[] { fwd.x, fwd.y, fwd.z },
                radius    = _radius,
                importance = (uint)_importance
            };
        }

        private static float ComputeAverageGain(float[] bands)
        {
            if (bands == null || bands.Length == 0) return 1f;
            float sum = 0f;
            for (int i = 0; i < bands.Length; i++) sum += bands[i];
            return sum / bands.Length;
        }

        private static float ComputeAverage(float[] values)
        {
            if (values == null || values.Length == 0) return 0f;
            float sum = 0f;
            for (int i = 0; i < values.Length; i++) sum += values[i];
            return sum / values.Length;
        }

        // ----- Gizmos (Editor) ---------------------------------------------
#if UNITY_EDITOR
        private void OnDrawGizmosSelected()
        {
            Gizmos.color = Color.cyan;
            Gizmos.DrawWireSphere(transform.position, _radius);

            Gizmos.color = Color.yellow;
            Gizmos.DrawRay(transform.position, transform.forward * 2f);

            if (_registered && _lastResult.direct.direction != null)
            {
                Gizmos.color = Color.green;
                Vector3 directDir = new Vector3(
                    _lastResult.direct.direction[0],
                    _lastResult.direct.direction[1],
                    _lastResult.direct.direction[2]);
                Gizmos.DrawRay(transform.position, directDir * 1.5f);
            }
        }
#endif
    }
}
