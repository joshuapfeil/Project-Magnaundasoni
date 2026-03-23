// ============================================================================
// MagnaundasoniListener.cs – Acoustic listener component
// ============================================================================
using System;
using UnityEngine;

namespace Magnaundasoni
{
    [DisallowMultipleComponent]
    [AddComponentMenu("Magnaundasoni/Acoustic Listener")]
    [RequireComponent(typeof(AudioListener))]
    public class MagnaundasoniListener : MonoBehaviour
    {
        // ----- Static Active Listener Tracking -----------------------------
        private static MagnaundasoniListener _activeListener;
        public static MagnaundasoniListener ActiveListener => _activeListener;

        // ----- Public Properties -------------------------------------------
        public uint NativeListenerID => _listenerID;
        public bool IsRegistered => _registered;

        // ----- Private State -----------------------------------------------
        private uint _listenerID;
        private bool _registered;

        // ----- Lifecycle ---------------------------------------------------
        private void OnEnable()
        {
            _activeListener = this;
            Register();
        }

        private void OnDisable()
        {
            Unregister();
            if (_activeListener == this)
                _activeListener = null;
        }

        private void Update()
        {
            if (!_registered) return;

            var engine = MagnaundasoniEngine.Current;
            if (engine == null) return;
            if (!engine.IsInitialized) return;

            UpdateNativePosition(engine);
        }

        private void OnAudioFilterRead(float[] data, int channels)
        {
            if (data == null || data.Length == 0 || channels <= 0) return;
            if (_activeListener != this || !_registered) return;

            var engine = MagnaundasoniEngine.Current;
            if (engine == null || !engine.IsInitialized) return;
            if (engine.CurrentMode != RenderingMode.BuiltIn) return;

            try
            {
                MagAPI.RenderAudio(engine.NativeHandle, _listenerID,
                    data, (uint)(data.Length / channels), (uint)channels,
                    (uint)AudioSettings.outputSampleRate);
            }
            catch (MagnaundasoniException)
            {
                Array.Clear(data, 0, data.Length);
            }
        }

        // ----- Registration ------------------------------------------------
        private void Register()
        {
            var engine = MagnaundasoniEngine.Instance;
            if (!engine.IsInitialized) return;

            var desc = BuildListenerDesc();
            try
            {
                _listenerID = MagAPI.ListenerRegister(engine.NativeHandle, desc);
                _registered = true;
            }
            catch (MagnaundasoniException ex)
            {
                Debug.LogError($"[Magnaundasoni] Listener register failed: {ex.Message}");
            }
        }

        private void Unregister()
        {
            if (!_registered) return;

            var engine = MagnaundasoniEngine.Current;
            if (engine != null && engine.IsInitialized)
            {
                try { MagAPI.ListenerUnregister(engine.NativeHandle, _listenerID); }
                catch (MagnaundasoniException) { }
            }
            _registered = false;
        }

        // ----- Per-Frame Updates -------------------------------------------
        private void UpdateNativePosition(MagnaundasoniEngine engine)
        {
            var desc = BuildListenerDesc();
            try { MagAPI.ListenerUpdate(engine.NativeHandle, _listenerID, desc); }
            catch (MagnaundasoniException) { }
        }

        private MagListenerDesc BuildListenerDesc()
        {
            Vector3 pos = transform.position;
            Vector3 fwd = transform.forward;
            Vector3 up  = transform.up;
            return new MagListenerDesc
            {
                position = new float[] { pos.x, pos.y, pos.z },
                forward  = new float[] { fwd.x, fwd.y, fwd.z },
                up       = new float[] { up.x,  up.y,  up.z  }
            };
        }

        // ----- Gizmos (Editor) ---------------------------------------------
#if UNITY_EDITOR
        private void OnDrawGizmosSelected()
        {
            Gizmos.color = new Color(0.2f, 0.8f, 1f, 0.5f);
            Gizmos.DrawSphere(transform.position, 0.15f);

            Gizmos.color = Color.blue;
            Gizmos.DrawRay(transform.position, transform.forward * 1.5f);

            Gizmos.color = Color.green;
            Gizmos.DrawRay(transform.position, transform.up * 0.75f);
        }
#endif
    }
}
